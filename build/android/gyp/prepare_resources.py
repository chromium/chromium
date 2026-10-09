#!/usr/bin/env python3
#
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Process Android resource directories to generate .resources.zip and R.txt
files."""

import argparse
import os
import shutil
import sys

from util import build_utils
from util import jar_info_utils
from util import md5_check
from util import resource_utils
from util import resources_parser
import action_helpers  # build_utils adds //build to sys.path.
import zip_helpers


def _ParseArgs(args):
    """Parses command line options.

    Returns:
      An options object as from argparse.ArgumentParser.parse_args()
    """
    parser = argparse.ArgumentParser(description=__doc__)
    action_helpers.add_depfile_arg(parser)

    parser.add_argument(
        '--res-sources-path',
        required=True,
        help='Path to a list of input resources for this target.',
    )

    parser.add_argument(
        '--r-text-in',
        help='Path to pre-existing R.txt. Its resource IDs override those found '
        'in the generated R.txt when generating R.java.',
    )

    parser.add_argument(
        '--allow-missing-resources',
        action='store_true',
        help='Do not fail if some resources exist in the res/ dir but are not '
        'listed in the sources.',
    )

    parser.add_argument(
        '--resource-zip-out',
        help='Path to a zip archive containing all resources from '
        '--resource-dirs, merged into a single directory tree.',
    )

    parser.add_argument(
        '--r-text-out', help='Path to store the generated R.txt file.'
    )

    parser.add_argument(
        '--strip-drawables',
        action="store_true",
        help='Remove drawables from the resources.',
    )

    options = parser.parse_args(args)

    with open(options.res_sources_path, encoding='utf-8') as f:
        options.sources = f.read().splitlines()
    options.resource_dirs = resource_utils.DeduceResourceDirsFromFileList(
        options.sources
    )

    return options


def _CheckAllFilesListed(resource_files, resource_dirs):
    resource_files = set(resource_files)
    missing_files = []
    for path, _ in resource_utils.IterResourceFilesInDirectories(resource_dirs):
        if path not in resource_files:
            missing_files.append(path)

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


def _ZipResources(resource_dirs, zip_path, ignore_pattern):
    # ignore_pattern is a string of ':' delimited list of globs used to ignore
    # files that should not be part of the final resource zip.
    files_to_zip = []
    seen_archive_paths = set()
    path_info = resource_utils.ResourceInfoFile()
    for index, resource_dir in enumerate(resource_dirs):
        attributed_aar = None
        if not resource_dir.startswith('..'):
            aar_source_info_path = os.path.join(
                os.path.dirname(resource_dir), 'source.info'
            )
            if os.path.exists(aar_source_info_path):
                attributed_aar = jar_info_utils.ReadAarSourceInfo(
                    aar_source_info_path
                )

        existing_dirs = set(os.listdir(resource_dir))
        renamed_dirs = {}
        for path, archive_path in resource_utils.IterResourceFilesInDirectories(
            [resource_dir], ignore_pattern
        ):
            attributed_path = path
            if attributed_aar:
                attributed_path = os.path.join(
                    attributed_aar, 'res', path[len(resource_dir) + 1 :]
                )
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

    path_info.Write(zip_path + '.info')
    zip_helpers.add_files_to_zip(files_to_zip, zip_path)


def _GenerateRTxt(options, r_txt_path):
    """Generate R.txt file.

    Args:
      options: The command-line options tuple.
      r_txt_path: Locates where the R.txt file goes.
    """
    ignore_pattern = resource_utils.AAPT_IGNORE_PATTERN
    if options.strip_drawables:
        ignore_pattern += ':*drawable*'

    resources_parser.RTxtGenerator(
        options.resource_dirs, ignore_pattern
    ).WriteRTxtFile(r_txt_path)


def _OnStaleMd5(options):
    with resource_utils.BuildContext() as build:
        if options.sources and not options.allow_missing_resources:
            _CheckAllFilesListed(options.sources, options.resource_dirs)
        if options.r_text_in:
            r_txt_path = options.r_text_in
        else:
            _GenerateRTxt(options, build.r_txt_path)
            r_txt_path = build.r_txt_path

        if options.r_text_out:
            shutil.copyfile(r_txt_path, options.r_text_out)

        if options.resource_zip_out:
            ignore_pattern = resource_utils.AAPT_IGNORE_PATTERN
            if options.strip_drawables:
                ignore_pattern += ':*drawable*'
            _ZipResources(
                options.resource_dirs, options.resource_zip_out, ignore_pattern
            )


def main(args):
    args = build_utils.ExpandFileArgs(args)
    options = _ParseArgs(args)

    # Order of these must match order specified in GN so that the correct one
    # appears first in the depfile.
    output_paths = [
        options.resource_zip_out,
        options.resource_zip_out + '.info',
        options.r_text_out,
    ]

    input_paths = [options.res_sources_path]
    if options.r_text_in:
        input_paths += [options.r_text_in]

    # Resource files aren't explicitly listed in GN. Listing them in the depfile
    # ensures the target will be marked stale when resource files are removed.
    depfile_deps = []
    resource_names = []
    for resource_dir in options.resource_dirs:
        for resource_file in build_utils.FindInDirectory(resource_dir, '*'):
            # Don't list the empty .keep file in depfile. Since it doesn't end up
            # included in the .zip, it can lead to -w 'dupbuild=err' ninja errors
            # if ever moved.
            if not resource_file.endswith(os.path.join('empty', '.keep')):
                input_paths.append(resource_file)
                depfile_deps.append(resource_file)
            resource_names.append(os.path.relpath(resource_file, resource_dir))

    # Resource filenames matter to the output, so add them to strings as well.
    # This matters if a file is renamed but not changed (http://crbug.com/597126).
    input_strings = sorted(resource_names) + [
        options.strip_drawables,
    ]

    # Since android_resources targets like *__all_dfm_resources depend on java
    # targets that they do not need (in reality it only needs the transitive
    # resource targets that those java targets depend on), md5_check is used to
    # prevent outputs from being re-written when real inputs have not changed.
    md5_check.CallAndWriteDepfileIfStale(
        lambda: _OnStaleMd5(options),
        options,
        input_paths=input_paths,
        input_strings=input_strings,
        output_paths=output_paths,
        depfile_deps=depfile_deps,
    )


if __name__ == '__main__':
    main(sys.argv[1:])
