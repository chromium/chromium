#!/usr/bin/env python3
#
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Process Android resource directories to generate .resources.zip and R.txt
files."""

import argparse
import hashlib
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

from util import build_utils
from util import jar_info_utils
from util import parallel
from util import resource_utils
from util import resources_parser
import zip_helpers  # build_utils adds //build to sys.path.

# Pngs that we shouldn't convert to webp. Please add rationale when updating.
_PNG_WEBP_EXCLUSION_PATTERN = re.compile(
    '|'.join(
        [
            # Android requires pngs for 9-patch images.
            r'.*\.9\.png',
            # Daydream requires pngs for icon files.
            r'.*daydream_icon_.*\.png',
        ]
    )
)


def _ParseArgs(args):
    """Parses command line options.

    Returns:
      An options object as from argparse.ArgumentParser.parse_args()
    """
    parser = argparse.ArgumentParser(description=__doc__)

    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument(
        '--res-sources-path',
        help='Path to a list of input resources for this target.',
    )
    input_group.add_argument(
        '--resources-zip-path',
        help='Path to an uncompiled input resources zip.',
    )

    parser.add_argument(
        '--r-text-in',
        help='Path to pre-existing R.txt. Its resource IDs override those found '
        'in the generated R.txt when generating R.java.',
    )

    parser.add_argument(
        '--aapt2-path',
        required=True,
        help='Path to the Android aapt2 tool.',
    )

    parser.add_argument(
        '--resource-zip-out',
        help='Path to a zip archive containing all compiled resources.',
    )

    parser.add_argument(
        '--r-text-out', help='Path to store the generated R.txt file.'
    )

    parser.add_argument(
        '--strip-drawables',
        action="store_true",
        help='Remove drawables from the resources.',
    )
    parser.add_argument(
        '--webp-binary', default='', help='Path to the cwebp binary.'
    )
    parser.add_argument(
        '--webp-cache-dir', help='The directory to store webp image cache.'
    )

    options = parser.parse_args(args)

    if options.res_sources_path:
        with open(options.res_sources_path, encoding='utf-8') as f:
            options.sources = f.read().splitlines()
        options.resource_dirs = resource_utils.DeduceResourceDirsFromFileList(
            options.sources
        )

    return options


def _CollectFilesByDirectory(resource_dirs):
    """Walks each resource directory once.

    Returns:
      Dict mapping each resource_dir to a list of (path, archive_path) tuples.
    """
    files_by_dir = {}
    for resource_dir in resource_dirs:
        entries = []
        for root, _, files in os.walk(resource_dir):
            parent_dir = os.path.relpath(root, resource_dir)
            for f in files:
                archive_path = (
                    f if parent_dir == '.' else os.path.join(parent_dir, f)
                )
                entries.append((os.path.join(root, f), archive_path))
        files_by_dir[resource_dir] = entries
    return files_by_dir


def _FilterFilesByDirectory(files_by_dir, ignore_pattern):
    globs = resource_utils.GenerateGlobs(ignore_pattern)
    ret = {}
    for resource_dir, entries in files_by_dir.items():
        active_entries = [
            (path, archive_path)
            for path, archive_path in entries
            if not (
                build_utils.MatchesGlob(archive_path, globs)
                or build_utils.MatchesGlob(
                    os.path.basename(archive_path), globs
                )
            )
        ]
        if active_entries:
            ret[resource_dir] = active_entries
    return ret


def _CheckAllFilesListed(resource_files, active_files_by_dir):
    resource_files = set(resource_files)
    missing_files = [
        path
        for entries in active_files_by_dir.values()
        for path, _ in entries
        if path not in resource_files
    ]

    if missing_files:
        sys.stderr.write(
            'Error: Found files not listed in the sources list of '
            'the BUILD.gn target:\n'
        )
        for path in missing_files:
            sys.stderr.write('{}\n'.format(path))
        sys.exit(1)


def _TransformArchivePath(archive_path, existing_dirs, renamed_dirs):
    """Applies static resource path transformations before zipping.

    Specifically:
      * Renames non-standard locale resource directories into standard Android
        locale names (e.g. values-fil -> values-tl, values-b+en+US ->
        values-en-rUS, values-id -> values-in, values-no -> values-nb) unless
        the target config directory already exists in the same resource_dir.
      * Moves images from drawable-*-mdpi-* folders to drawable-* folders (see
        http://crbug.com/289843).
      * Removes .png and .webp extensions (except .9.png) to save binary size
        and skip redundant aapt2 PNG crunching.
    """
    locale = resource_utils.FindLocaleInStringResourceFilePath(archive_path)
    if locale:
        cr_locale = resource_utils.ToChromiumLocaleName(locale)
        if not cr_locale:
            return archive_path
        locale2 = resource_utils.ToAndroidLocaleName(cr_locale)
        if locale == locale2:
            return archive_path
        src_subdir = f'values-{locale}'
        dst_subdir = renamed_dirs.get(src_subdir)
        if dst_subdir is None:
            # Ignore rather than rename when the destination resources config
            # already exists (e.g. some libraries provide both values-nb/ and
            # values-no/, or both values-es-rUS/ and values-b+es+419/).
            target_subdir = f'values-{locale2}'
            if target_subdir in existing_dirs:
                dst_subdir = src_subdir
            else:
                dst_subdir = target_subdir
                existing_dirs.add(dst_subdir)
            renamed_dirs[src_subdir] = dst_subdir
        return f'{dst_subdir}/{os.path.basename(archive_path)}'

    subdir_name, filename = os.path.split(archive_path)
    src_components = subdir_name.split('-')
    if src_components[0] == 'drawable' and 'mdpi' in src_components:
        dst_components = [c for c in src_components if c != 'mdpi']
        subdir_name = '-'.join(dst_components)
        archive_path = f'{subdir_name}/{filename}'

    # Strip .png and .webp extensions (except .9.png and raw/):
    # 1. Android's resource loader only checks for .xml and .9.png extensions,
    #    whereas `aapt2 optimize --shorten-resource-paths` preserves extensions
    #    (e.g. "res/xo.png"). Stripping them saves 4-5 bytes per image across
    #    the resources.arsc string pool and APK zip headers (crbug.com/1014555).
    # 2. `aapt2 compile` runs PNG crunching whenever a file ends with ".png"
    #    (and we cannot pass --no-crunch because .9.png files must be crunched).
    #    Stripping ".png" beforehand causes aapt2 to take the fast raw-file
    #    copy path for regular PNGs while still crunching .9.png files.
    if (
        not archive_path.startswith('raw/')
        and (archive_path.endswith('.png') or archive_path.endswith('.webp'))
        and not archive_path.endswith('.9.png')
    ):
        archive_path = os.path.splitext(archive_path)[0]

    return archive_path


def _ConvertToWebPSingle(png_path, cwebp_binary, cwebp_version, webp_cache_dir):
    with open(png_path, 'rb') as f:
        sha1_hash = hashlib.sha1(f.read()).hexdigest()

    # The set of arguments that will appear in the cache key.
    quality_args = ['-m', '6', '-q', '100', '-lossless']

    webp_cache_path = os.path.join(
        webp_cache_dir,
        '{}-{}-{}'.format(sha1_hash, cwebp_version, ''.join(quality_args)),
    )

    cache_hit = os.path.exists(webp_cache_path)
    if not cache_hit:
        with tempfile.NamedTemporaryFile(
            dir=webp_cache_dir, suffix='.webp', delete=False
        ) as webp_tmp:
            webp_tmp_path = webp_tmp.name
        try:
            args = [
                cwebp_binary,
                png_path,
                '-o',
                webp_tmp_path,
                '-quiet',
            ] + quality_args
            subprocess.check_call(args)
            os.replace(webp_tmp_path, webp_cache_path)
        except BaseException:
            if os.path.exists(webp_tmp_path):
                os.unlink(webp_tmp_path)
            raise

    return png_path, webp_cache_path, cache_hit


def _ConvertToWebP(cwebp_binary, png_paths, webp_cache_dir):
    cwebp_version = subprocess.check_output([cwebp_binary, '-version']).rstrip()
    build_utils.MakeDirectory(webp_cache_dir)
    results = parallel.BulkForkAndCall(
        _ConvertToWebPSingle,
        [(f,) for f in png_paths],
        cwebp_binary=cwebp_binary,
        cwebp_version=cwebp_version,
        webp_cache_dir=webp_cache_dir,
    )
    webp_map = {}
    total_cache_hits = 0
    for png_path, webp_cache_path, cache_hit in results:
        webp_map[png_path] = webp_cache_path
        total_cache_hits += int(cache_hit)

    logging.debug('png->webp cache: %d/%d', total_cache_hits, len(png_paths))
    return webp_map


def _CompileResourceZip(
    aapt2_path, input_zip_path, output_zip_path, before_join_callback=None
):
    """Compiles a zip of raw Android resources into a zip of .flat files."""
    with zipfile.ZipFile(input_zip_path) as z:
        if not z.namelist():
            shutil.copyfile(input_zip_path, output_zip_path)
            if before_join_callback:
                before_join_callback()
            return
    compile_command = [
        aapt2_path,
        'compile',
        # TODO(wnwen): Turn this on once aapt2 forces 9-patch to be crunched.
        # '--no-crunch',
        '--zip',
        input_zip_path,
        '-o',
        output_zip_path,
    ]
    # There are resources targeting API-versions lower than our minapi. For
    # various reasons it's easier to let aapt2 ignore these than for us to
    # remove them from our build (e.g. it's from a 3rd party library).
    build_utils.CheckOutput(
        compile_command,
        env=resource_utils.Aapt2Env(),
        stderr_filter=lambda output: build_utils.FilterLines(
            output, r'ignoring configuration .* for (styleable|attribute)'
        ),
        before_join_callback=before_join_callback,
    )


def _ZipResources(
    active_files_by_dir,
    zip_path,
    aapt2_path,
    webp_binary=None,
    webp_cache_dir=None,
    before_join_callback=None,
):
    files_to_zip = []
    png_paths = []
    seen_archive_paths = set()
    path_info = resource_utils.ResourceInfoFile()
    for index, (resource_dir, entries) in enumerate(
        active_files_by_dir.items()
    ):
        attributed_aar = None
        if not resource_dir.startswith('..'):
            aar_source_info_path = os.path.join(
                os.path.dirname(resource_dir), 'source.info'
            )
            if os.path.exists(aar_source_info_path):
                attributed_aar = jar_info_utils.ReadAarSourceInfo(
                    aar_source_info_path
                )

        existing_dirs = {
            os.path.dirname(archive_path) for _, archive_path in entries
        }
        renamed_dirs = {}
        for path, archive_path in entries:
            attributed_path = path
            if attributed_aar:
                attributed_path = os.path.join(
                    attributed_aar, 'res', archive_path
                )
            if (
                webp_binary
                and not archive_path.startswith('raw/')
                and archive_path.endswith('.png')
                and not _PNG_WEBP_EXCLUSION_PATTERN.match(archive_path)
            ):
                png_paths.append(path)
            archive_path = _TransformArchivePath(
                archive_path, existing_dirs, renamed_dirs
            )
            path_info.AddMapping(archive_path, attributed_path)

            # Allow a single target to have multiple res/ directories with
            # identically named values*/ XML files (e.g. values/strings.xml).
            # Non-values collisions are already rejected by AddMapping() above,
            # and aapt2 ignores the XML filename for values*/ resources.
            if archive_path in seen_archive_paths:
                subdir, filename = os.path.split(archive_path)
                archive_path = f'{subdir}/{index}_{filename}'
            seen_archive_paths.add(archive_path)
            files_to_zip.append((archive_path, path))

    if png_paths:
        webp_map = _ConvertToWebP(webp_binary, png_paths, webp_cache_dir)
        files_to_zip = [
            (archive_path, webp_map.get(path, path))
            for archive_path, path in files_to_zip
        ]

    path_info.Write(zip_path + '.info')
    # `aapt2 compile --dir` cannot be used directly on `resource_dirs` because:
    # 1. It only accepts a single directory, whereas targets can have multiple.
    # 2. It has no ignore-pattern flag for AAPT_IGNORE_PATTERN /
    #    --strip-drawables.
    # 3. _TransformArchivePath() renames subdirectories and files (locale
    #    normalization, removing -mdpi, stripping .png/.webp extensions, and
    #    disambiguating duplicate values*/ filenames across resource_dirs).
    # Writing an uncompressed raw.zip and passing `--zip` is also faster than
    # staging a directory tree of symlinks/copies for `--dir`, and embeds clean
    # zip-relative paths in .flat headers rather than temp_dir paths
    # (https://crbug.com/939984).
    with build_utils.TempDir() as temp_dir:
        raw_zip_path = os.path.join(temp_dir, 'raw.zip')
        zip_helpers.add_files_to_zip(files_to_zip, raw_zip_path)
        _CompileResourceZip(
            aapt2_path,
            raw_zip_path,
            zip_path,
            before_join_callback=before_join_callback,
        )


def _GenerateRTxt(
    options,
    active_files_by_dir=None,
    ignore_pattern=resource_utils.AAPT_IGNORE_PATTERN,
):
    """Generate R.txt file."""
    if not options.r_text_out:
        return
    if options.r_text_in:
        shutil.copyfile(options.r_text_in, options.r_text_out)
    elif options.resources_zip_path:
        resources_parser.RTxtGenerator(
            res_zips=[options.resources_zip_path]
        ).WriteRTxtFile(options.r_text_out)
    else:
        resources_parser.RTxtGenerator(
            active_files_by_dir.keys(), ignore_pattern
        ).WriteRTxtFile(options.r_text_out)


def main(args):
    args = build_utils.ExpandFileArgs(args)
    options = _ParseArgs(args)

    if options.resources_zip_path:
        if options.resource_zip_out:
            _CompileResourceZip(
                options.aapt2_path,
                options.resources_zip_path,
                options.resource_zip_out,
                before_join_callback=lambda: _GenerateRTxt(options),
            )
        else:
            _GenerateRTxt(options)
        return

    files_by_dir = _CollectFilesByDirectory(options.resource_dirs)
    ignore_pattern = resource_utils.AAPT_IGNORE_PATTERN
    active_files_by_dir = _FilterFilesByDirectory(files_by_dir, ignore_pattern)
    if options.sources:
        _CheckAllFilesListed(options.sources, active_files_by_dir)

    if options.strip_drawables:
        ignore_pattern += ':*drawable*'
        active_files_by_dir = _FilterFilesByDirectory(
            active_files_by_dir, ignore_pattern
        )

    if options.resource_zip_out:
        _ZipResources(
            active_files_by_dir,
            options.resource_zip_out,
            options.aapt2_path,
            webp_binary=options.webp_binary,
            webp_cache_dir=options.webp_cache_dir,
            before_join_callback=lambda: _GenerateRTxt(
                options, active_files_by_dir, ignore_pattern
            ),
        )
    else:
        _GenerateRTxt(options, active_files_by_dir, ignore_pattern)


if __name__ == '__main__':
    main(sys.argv[1:])
