#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Create an Android application bundle from one or more bundle modules."""

import argparse
import collections
import concurrent.futures
import json
import logging
import os
import posixpath
import sys
import zipfile

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
)
from pylib.dex import dex_parser

import bundletool
from util import build_utils
from util import manifest_utils
from util import protoresources
from util import zipbuilder
import action_helpers  # build_utils adds //build to sys.path.

# List of split dimensions recognized by this tool.
_ALL_SPLIT_DIMENSIONS = ['ABI', 'SCREEN_DENSITY', 'LANGUAGE']

# Matches bundletool's Deflater.DEFAULT_COMPRESSION for official builds.
_DEFAULT_COMPRESS_LEVEL = 6

# A list of extensions corresponding to files that should never be compressed
# in the bundle. This used to be handled by bundletool automatically until
# release 0.8.0, which required that this be passed to the BundleConfig
# file instead.
#
# This is the original list, which was taken from aapt2, with 'webp' added to
# it (which curiously was missing from the list).
_UNCOMPRESSED_FILE_EXTS = [
    '3g2',
    '3gp',
    '3gpp',
    '3gpp2',
    'aac',
    'amr',
    'awb',
    'git',
    'imy',
    'jet',
    'jpeg',
    'jpg',
    'm4a',
    'm4v',
    'mid',
    'midi',
    'mkv',
    'mp2',
    'mp3',
    'mp4',
    'mpeg',
    'mpg',
    'ogg',
    'png',
    'rtttl',
    'smf',
    'wav',
    'webm',
    'webp',
    'wmv',
    'xmf',
]

_COMPONENT_TYPES = ('activity', 'provider', 'receiver', 'service')
_DEDUPE_ENTRY_TYPES = _COMPONENT_TYPES + ('activity-alias', 'meta-data')

_ROTATION_METADATA_KEY = 'com.google.play.apps.signing/RotationConfig.textproto'
_JAVALESS_SERVICE_NAME = (
    'org.chromium.content.app.NativeOnlySandboxedProcessService'
)

_ALLOWLISTED_NON_BASE_SERVICES = {
    # Only on API level 33+ which is past the fix for b/169196314.
    'androidx.pdf.service.PdfDocumentServiceImpl',
    'androidx.pdf.service.PdfDocumentService',
    "com.google.android.apps.chrome.browser.media.ChromeAppContentProjectionService",
    # Not an actual service; used only as a metadata container for ComponentDiscovery.
    'com.google.mlkit.common.internal.MlKitComponentDiscoveryService',
    # These need to be burned down - these have likely never fully worked.
    'com.google.apps.tiktok.concurrent.AndroidFuturesService',
    'com.google.apps.tiktok.concurrent.InternalForegroundService',
}


def _ParseArgs(args):
    parser = argparse.ArgumentParser()
    action_helpers.add_depfile_arg(parser)
    parser.add_argument(
        '--out-bundle', required=True, help='Output bundle zip archive.'
    )
    parser.add_argument(
        '--module-zips',
        required=True,
        help='GN-list of skeleton module zip archives.',
    )
    parser.add_argument(
        '--module-specs',
        required=True,
        help='GN-list of module ZipBuilder spec files.',
    )
    parser.add_argument(
        '--pathmap-in-paths',
        action='append',
        help='List of module pathmap files.',
    )
    parser.add_argument(
        '--module-name',
        action='append',
        dest='module_names',
        help='List of module names.',
    )
    parser.add_argument(
        '--pathmap-out-path', help='Path to combined pathmap file for bundle.'
    )
    parser.add_argument(
        '--rtxt-in-paths',
        action='append',
        help='GN-list of module R.txt files.',
    )
    parser.add_argument(
        '--rtxt-out-path', help='Path to combined R.txt file for bundle.'
    )
    parser.add_argument(
        '--uncompressed-assets',
        action='append',
        help='GN-list of uncompressed assets.',
    )
    parser.add_argument(
        '--compress-dex', action='store_true', help='Compress .dex files'
    )
    parser.add_argument(
        '--uncompressed',
        action='store_true',
        help='Assemble the .aab without recompressing module entries (faster, '
        'larger).',
    )
    parser.add_argument(
        '--split-dimensions', help="GN-list of split dimensions to support."
    )
    parser.add_argument(
        '--rotation-config', help='Path to a RotationConfig.textproto'
    )
    parser.add_argument(
        '--warnings-as-errors',
        action='store_true',
        help='Treat all warnings as errors.',
    )

    parser.add_argument(
        '--validate-services',
        action='store_true',
        help='Check if services are in base module if isolatedSplits is enabled.',
    )

    options = parser.parse_args(args)
    options.module_zips = action_helpers.parse_gn_list(options.module_zips)
    options.module_specs = action_helpers.parse_gn_list(options.module_specs)

    if len(options.module_zips) == 0:
        parser.error('The module zip list cannot be empty.')
    if len(options.module_zips) != len(options.module_names):
        parser.error('# module zips != # names.')
    if len(options.module_specs) != len(options.module_names):
        parser.error('# module specs != # names.')
    if 'base' not in options.module_names:
        parser.error('Missing base module.')

    # Sort modules for more stable outputs.
    per_module_values = list(
        zip(
            options.module_names,
            options.module_zips,
            options.module_specs,
            options.uncompressed_assets,
            options.rtxt_in_paths,
            options.pathmap_in_paths,
        )
    )
    per_module_values.sort(key=lambda x: (x[0] != 'base', x[0]))
    options.module_names = [x[0] for x in per_module_values]
    options.module_zips = [x[1] for x in per_module_values]
    options.module_specs = [x[2] for x in per_module_values]
    options.uncompressed_assets = [x[3] for x in per_module_values]
    options.rtxt_in_paths = [x[4] for x in per_module_values]
    options.pathmap_in_paths = [x[5] for x in per_module_values]

    options.rtxt_in_paths = action_helpers.parse_gn_list(options.rtxt_in_paths)
    options.pathmap_in_paths = action_helpers.parse_gn_list(
        options.pathmap_in_paths
    )

    # Merge all uncompressed assets into a set.
    uncompressed_list = []
    for entry in action_helpers.parse_gn_list(options.uncompressed_assets):
        # Each entry has the following format: 'zipPath' or 'srcPath:zipPath'
        pos = entry.find(':')
        if pos >= 0:
            uncompressed_list.append(entry[pos + 1 :])
        else:
            uncompressed_list.append(entry)

    options.uncompressed_assets = set(uncompressed_list)

    # Check that all split dimensions are valid
    if options.split_dimensions:
        options.split_dimensions = action_helpers.parse_gn_list(
            options.split_dimensions
        )
        for dim in options.split_dimensions:
            if dim.upper() not in _ALL_SPLIT_DIMENSIONS:
                parser.error(
                    'Invalid split dimension "%s" (expected one of: %s)'
                    % (dim, ', '.join(x.lower() for x in _ALL_SPLIT_DIMENSIONS))
                )

    return options


def _MakeSplitDimension(value, enabled):
    """Return dict modelling a BundleConfig splitDimension entry."""
    return {'value': value, 'negate': not enabled}


def _GenerateBundleConfigJson(
    uncompressed_assets,
    compress_dex,
    split_dimensions,
):
    """Generate a dictionary that can be written to a JSON BuildConfig.

    Args:
      uncompressed_assets: A list or set of file paths under assets/ that always
        be stored uncompressed.
      compressed_dex: Boolean, whether to compress .dex.
      split_dimensions: list of split dimensions.
    Returns:
      A dictionary that can be written as a json file.
    """
    # Compute splitsConfig list. Each item is a dictionary that can have
    # the following keys:
    #    'value': One of ['LANGUAGE', 'DENSITY', 'ABI']
    #    'negate': Boolean, True to indicate that the bundle should *not* be
    #              split (unused at the moment by this script).

    split_dimensions = [
        _MakeSplitDimension(dim, dim in split_dimensions)
        for dim in _ALL_SPLIT_DIMENSIONS
    ]

    # normpath to allow for ../ prefix.
    uncompressed_globs = [
        posixpath.normpath('assets/' + x) for x in uncompressed_assets
    ]
    # NOTE: Use '**' instead of '*' to work through directories!
    uncompressed_globs.extend('**.' + ext for ext in _UNCOMPRESSED_FILE_EXTS)
    if not compress_dex:
        # Bundletool honors "uncompressDexFiles" for split APKs, but ignores it
        # for --mode=universal. Add a placeholder entry that has no effect
        # unless rewritten by app_bundle_utils.GenerateBundleApks() for
        # universal mode.
        uncompressed_globs.append('classesX.dex')

    data = {
        'optimizations': {
            'resourceOptimizations': {
                'collapsedResourceNames': {
                    'deduplicateResourceEntries': True,
                },
                'sparseEncoding': 'VARIANT_FOR_SDK_32',
            },
            'splitsConfig': {
                'splitDimension': split_dimensions,
            },
            'uncompressNativeLibraries': {
                'enabled': True,
                'alignment': 'PAGE_ALIGNMENT_16K',
            },
            'uncompressDexFiles': {
                'enabled': not compress_dex,  # Applies only for Q+.
            },
            'inject_min_sdk': True,  # Required for sparseEncoding
        },
        'compression': {
            'uncompressedGlob': sorted(uncompressed_globs),
        },
    }

    return json.dumps(data, indent=2)


def _ConcatTextFiles(in_paths, out_path):
    """Concatenate the contents of multiple text files into one.

    The each file contents is preceded by a line containing the original filename.

    Args:
      in_paths: List of input file paths.
      out_path: Path to output file.
    """
    with open(out_path, 'w', encoding='utf-8') as out_file:
        for in_path in in_paths:
            if not os.path.exists(in_path):
                continue
            with open(in_path, 'r', encoding='utf-8') as in_file:
                out_file.write(
                    '-- Contents of {}\n'.format(os.path.basename(in_path))
                )
                out_file.write(in_file.read())


def _LoadPathmap(pathmap_path):
    """Load the pathmap of obfuscated resource paths.

    Returns: A dict mapping from obfuscated paths to original paths or an
             empty dict if passed a None |pathmap_path|.
    """
    if pathmap_path is None:
        return {}

    pathmap = {}
    with open(pathmap_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if line.startswith('--') or line == '':
                continue
            original, renamed = line.split(' -> ')
            pathmap[renamed] = original
    return pathmap


def _WriteBundlePathmap(
    module_pathmap_paths, module_names, bundle_pathmap_path
):
    """Combine the contents of module pathmaps into a bundle pathmap.

    This rebases the resource paths inside the module pathmap before adding them
    to the bundle pathmap. So res/a.xml inside the base module pathmap would be
    base/res/a.xml in the bundle pathmap.
    """
    with open(
        bundle_pathmap_path, 'w', encoding='utf-8'
    ) as bundle_pathmap_file:
        for module_pathmap_path, module_name in zip(
            module_pathmap_paths, module_names
        ):
            if not os.path.exists(module_pathmap_path):
                continue
            module_pathmap = _LoadPathmap(module_pathmap_path)
            for short_path, long_path in module_pathmap.items():
                rebased_long_path = '{}/{}'.format(module_name, long_path)
                rebased_short_path = '{}/{}'.format(module_name, short_path)
                line = '{} -> {}\n'.format(
                    rebased_long_path, rebased_short_path
                )
                bundle_pathmap_file.write(line)


def _GetManifestForModule(module_zip):
    with zipfile.ZipFile(module_zip) as z:
        data = z.read('manifest/AndroidManifest.xml')
    return protoresources.Resources_pb2.XmlNode.FromString(data).element


def _GetComponentNames(manifest, tag_name):
    names = []
    for app_node in manifest.child:
        if app_node.element.name != 'application':
            continue
        for child in app_node.element.child:
            if child.element.name != tag_name:
                continue
            for a in child.element.attribute:
                if (
                    a.namespace_uri == manifest_utils.ANDROID_NAMESPACE
                    and a.name == 'name'
                ):
                    names.append(a.value)
                    break
    return names


def _ClassesFromSpec(spec_entries):
    dex_by_jar = collections.defaultdict(list)
    for entry in spec_entries:
        if (
            entry.src_entry_name
            and entry.dest_name.startswith('dex/classes')
            and entry.dest_name.endswith('.dex')
        ):
            dex_by_jar[entry.src_path].append(entry.src_entry_name)
    classes = set()
    for jar_path, entry_names in dex_by_jar.items():
        with zipfile.ZipFile(jar_path) as z:
            for entry_name in entry_names:
                dexfile = dex_parser.DexFile(z.read(entry_name))
                for item in dexfile.class_def_item_list:
                    desc = dexfile.GetTypeString(item.class_idx)
                    classes.add(desc[1:-1].replace('/', '.'))
    return classes


def _IsJavalessService(service_name):
    return service_name.startswith(_JAVALESS_SERVICE_NAME)


def _IsIsolatedSplits(manifest):
    return any(
        a.namespace_uri == manifest_utils.ANDROID_NAMESPACE
        and a.name == 'isolatedSplits'
        and (a.value == 'true' or a.compiled_item.prim.boolean_value)
        for a in manifest.attribute
    )


def _AddBaseResourcesToSkeleton(base_skeleton_zip, base_resource_apk, tmp_dir):
    """Returns a copy of base's skeleton zip with resources.pb and res/* added.

    bundletool needs base's resources.pb (and res/* entries to satisfy
    ResourceTableValidator) when validating split module titles (multi-module
    bundles without isolatedSplits).
    """
    out_zip = os.path.join(tmp_dir, os.path.basename(base_skeleton_zip))
    with zipfile.ZipFile(base_skeleton_zip) as z:
        skeleton_names = z.namelist()
    with zipbuilder.ZipBuilder() as zip_builder:
        # The skeleton's manifest is a copy of the resource apk's. Adding it
        # from the resource apk excludes it from add_all_from_zip().
        zip_builder.add_from_zip(
            'manifest/AndroidManifest.xml',
            base_resource_apk,
            'AndroidManifest.xml',
        )
        zip_builder.add_all_from_zip(base_resource_apk)
        for name in skeleton_names:
            if name != 'manifest/AndroidManifest.xml':
                zip_builder.add_from_zip(name, base_skeleton_zip, name)
        zip_builder.write(out_zip)
    return out_zip


def _ValidateSplits(module_names, module_zips, base_spec):
    logging.info('Reading manifests and running dex_parser')
    manifests_by_name = {
        module_name: _GetManifestForModule(module_zip)
        for module_name, module_zip in zip(module_names, module_zips)
    }
    base_classes = _ClassesFromSpec(base_spec)

    # Collect service names from all split manifests.
    logging.info('Performing checks')
    errors = []

    # Ensure there are no components defined in multiple splits.
    splits_by_component = {}
    for module_name, cur_manifest in manifests_by_name.items():
        for kind in _DEDUPE_ENTRY_TYPES:
            for component in _GetComponentNames(cur_manifest, kind):
                owner_module_name = splits_by_component.setdefault(
                    (kind, component), module_name
                )
                # Allow services that exist only to keep <meta-data> out of
                # ApplicationInfo.
                if owner_module_name != module_name and not component.endswith(
                    'HolderService'
                ):
                    errors.append(
                        f'The {kind} "{component}" appeared in both '
                        f'{owner_module_name} and {module_name}.'
                    )

    # Ensure components defined in base manifest exist in base dex.
    for (kind, component), module_name in splits_by_component.items():
        if module_name == 'base' and kind in _COMPONENT_TYPES:
            if component not in base_classes and not _IsJavalessService(
                component
            ):
                errors.append(
                    f"{component} is defined in the base manfiest, "
                    f"but the class does not exist in the base splits' dex"
                )

    # Remaining checks apply only when isolatedSplits="true".
    if not _IsIsolatedSplits(manifests_by_name['base']):
        return errors

    # Ensure all providers are present in base module. We enforce this because
    # providers are loaded early in startup, and keeping them in the base module
    # gives more time for the chrome split to load.
    for module_name, cur_manifest in manifests_by_name.items():
        if module_name == 'base':
            continue
        provider_names = _GetComponentNames(cur_manifest, 'provider')
        for p in provider_names:
            errors.append(
                f'Provider {p} should be declared in the base manifest,'
                f' but is in "{module_name}" module. For details, see '
                'https://chromium.googlesource.com/chromium/src/+/main/'
                'docs/android_isolated_splits.md#contentproviders'
            )

    # Ensure all services are present in base module because service classes are
    # not found if they are not present in the base module. b/169196314
    # It is fine if they are defined in split manifests though.
    for module_name, cur_manifest in manifests_by_name.items():
        for service_name in _GetComponentNames(cur_manifest, 'service'):
            if (
                service_name not in base_classes
                and service_name not in _ALLOWLISTED_NON_BASE_SERVICES
                and not _IsJavalessService(service_name)
            ):
                errors.append(
                    f'Service {service_name} should be declared in the base'
                    f' manifest, but is in "{module_name}" module. For'
                    ' details, see b/169196314.'
                )

    return errors


def _IsBundletoolMetadataEntry(entry_name):
    if entry_name.endswith('/'):
        return False
    return (
        entry_name == 'BundleConfig.pb'
        or entry_name.startswith('BUNDLE-METADATA/')
        or (
            entry_name.count('/') == 1
            and entry_name.endswith(('/assets.pb', '/native.pb'))
        )
    )


def main(args):
    build_utils.InitLogging('AAB_DEBUG')
    args = build_utils.ExpandFileArgs(args)
    options = _ParseArgs(args)

    split_dimensions = []
    if options.split_dimensions:
        split_dimensions = [x.upper() for x in options.split_dimensions]

    with (
        build_utils.TempDir() as tmp_dir,
        action_helpers.atomic_output(
            options.out_bundle, only_if_changed=False
        ) as tmp_bundle_file,
        concurrent.futures.ThreadPoolExecutor() as executor,
    ):
        tmp_bundle = tmp_bundle_file.name

        logging.info('Creating BundleConfig.pb.json')
        bundle_config = _GenerateBundleConfigJson(
            options.uncompressed_assets,
            options.compress_dex,
            split_dimensions,
        )

        # Important: bundletool requires that the bundle config file is
        # named with a .pb.json extension.
        tmp_bundle_config = os.path.join(tmp_dir, 'BundleConfig.pb.json')

        with open(tmp_bundle_config, 'w', encoding='utf-8') as f:
            f.write(bundle_config)

        module_specs = [zipbuilder.parse_spec(p) for p in options.module_specs]

        if options.validate_services:
            validate_future = executor.submit(
                _ValidateSplits,
                options.module_names,
                options.module_zips,
                module_specs[0],
            )

        bundletool_modules = list(options.module_zips)
        if len(options.module_zips) > 1 and not _IsIsolatedSplits(
            _GetManifestForModule(options.module_zips[0])
        ):
            logging.info('Adding base resources.pb to skeleton module')
            base_resource_apk = next(
                e.src_path
                for e in module_specs[0]
                if e.src_entry_name
                and e.dest_name == 'manifest/AndroidManifest.xml'
            )
            bundletool_modules[0] = _AddBaseResourcesToSkeleton(
                options.module_zips[0], base_resource_apk, tmp_dir
            )
        bundletool_out = os.path.join(tmp_dir, 'skeleton_bundle.aab')

        logging.info('Running bundletool')
        cmd_args = build_utils.JavaCmd() + [
            '-jar',
            bundletool.BUNDLETOOL_JAR_PATH,
            'build-bundle',
            '--modules=' + ','.join(bundletool_modules),
            '--output=' + bundletool_out,
            '--overwrite',
            '--config=' + tmp_bundle_config,
            '--uncompressed=true',
        ]

        if options.rotation_config:
            cmd_args += [
                f'--metadata-file={_ROTATION_METADATA_KEY}:{options.rotation_config}'
            ]

        build_utils.CheckOutput(
            cmd_args,
            print_stdout=True,
            print_stderr=True,
            stderr_filter=build_utils.FilterReflectiveAccessJavaWarnings,
            fail_on_output=options.warnings_as_errors,
        )

        if options.validate_services:
            logging.info('Validating isolated split manifests')
            errors = validate_future.result()
            if errors:
                sys.stderr.write('Bundle failed sanity checks:\n  ')
                sys.stderr.write('\n  '.join(errors))
                sys.stderr.write('\n')
                sys.exit(1)

        logging.info('Writing output bundle with zipbuilder')
        compress_level = (
            zipbuilder.KEEP_COMPRESSION
            if options.uncompressed
            else _DEFAULT_COMPRESS_LEVEL
        )
        with zipbuilder.ZipBuilder() as zip_builder:
            for module_name, spec_entries in zip(
                options.module_names, module_specs
            ):
                zip_builder.add_spec_entries(
                    spec_entries,
                    dst_prefix=f'{module_name}/',
                    compress_level=compress_level,
                )
            with zipfile.ZipFile(bundletool_out) as z:
                for entry_name in z.namelist():
                    if _IsBundletoolMetadataEntry(entry_name):
                        zip_builder.add_from_zip(
                            entry_name,
                            bundletool_out,
                            entry_name,
                            compress_level=compress_level,
                        )
            zip_builder.write(
                tmp_bundle,
                # Compressing full bundles buffers deflated entries in parallel
                # across workers; match bundletool's 4G heap on official builds.
                xmx='1G' if options.uncompressed else '4G',
                warnings_as_errors=options.warnings_as_errors,
            )

        if not options.uncompressed:
            logging.info('Validating output bundle')
            bundletool.RunBundleTool(['validate', f'--bundle={tmp_bundle}'])

    if options.rtxt_out_path:
        _ConcatTextFiles(options.rtxt_in_paths, options.rtxt_out_path)

    if options.pathmap_out_path:
        _WriteBundlePathmap(
            options.pathmap_in_paths,
            options.module_names,
            options.pathmap_out_path,
        )

    if options.depfile:
        depfile_deps = {
            entry.src_path for spec in module_specs for entry in spec
        } - set(options.module_zips)
        for path in options.rtxt_in_paths + options.pathmap_in_paths:
            if path and os.path.exists(path):
                depfile_deps.add(path)
        action_helpers.write_depfile(
            options.depfile, options.out_bundle, inputs=list(depfile_deps)
        )


if __name__ == '__main__':
    main(sys.argv[1:])
