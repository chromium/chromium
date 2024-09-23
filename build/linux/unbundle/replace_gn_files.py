#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Replaces GN files in tree with files from here that
make the build use system libraries.
"""

import argparse
import os
import shutil
import sys


REPLACEMENTS = {
    # Use system libabsl_2xxx. These 20 shims MUST be used together.
    'absl_algorithm': 'third_party/abseil-cpp/absl/algorithm/BUILD.gn',
    'absl_base': 'third_party/abseil-cpp/absl/base/BUILD.gn',
    'absl_cleanup': 'third_party/abseil-cpp/absl/cleanup/BUILD.gn',
    'absl_container': 'third_party/abseil-cpp/absl/container/BUILD.gn',
    'absl_crc': 'third_party/abseil-cpp/absl/crc/BUILD.gn',
    'absl_debugging': 'third_party/abseil-cpp/absl/debugging/BUILD.gn',
    'absl_flags': 'third_party/abseil-cpp/absl/flags/BUILD.gn',
    'absl_functional': 'third_party/abseil-cpp/absl/functional/BUILD.gn',
    'absl_hash': 'third_party/abseil-cpp/absl/hash/BUILD.gn',
    'absl_log': 'third_party/abseil-cpp/absl/log/BUILD.gn',
    'absl_log_internal': 'third_party/abseil-cpp/absl/log/internal/BUILD.gn',
    'absl_memory': 'third_party/abseil-cpp/absl/memory/BUILD.gn',
    'absl_meta': 'third_party/abseil-cpp/absl/meta/BUILD.gn',
    'absl_numeric': 'third_party/abseil-cpp/absl/numeric/BUILD.gn',
    'absl_random': 'third_party/abseil-cpp/absl/random/BUILD.gn',
    'absl_status': 'third_party/abseil-cpp/absl/status/BUILD.gn',
    'absl_strings': 'third_party/abseil-cpp/absl/strings/BUILD.gn',
    'absl_synchronization':
    'third_party/abseil-cpp/absl/synchronization/BUILD.gn',
    'absl_time': 'third_party/abseil-cpp/absl/time/BUILD.gn',
    'absl_types': 'third_party/abseil-cpp/absl/types/BUILD.gn',
    'absl_utility': 'third_party/abseil-cpp/absl/utility/BUILD.gn',
    #
    'brotli': 'third_party/brotli/BUILD.gn',
    'crc32c': 'third_party/crc32c/BUILD.gn',
    'dav1d': 'third_party/dav1d/BUILD.gn',
    'double-conversion': 'base/third_party/double_conversion/BUILD.gn',
    'ffmpeg': 'third_party/ffmpeg/BUILD.gn',
    'flac': 'third_party/flac/BUILD.gn',
    'flatbuffers': 'third_party/flatbuffers/BUILD.gn',
    'fontconfig': 'third_party/fontconfig/BUILD.gn',
    'freetype': 'build/config/freetype/freetype.gni',
    'harfbuzz-ng': 'third_party/harfbuzz-ng/harfbuzz.gni',
    'highway': 'third_party/highway/BUILD.gn',
    'icu': 'third_party/icu/BUILD.gn',
    'jsoncpp': 'third_party/jsoncpp/BUILD.gn',
    'libaom': 'third_party/libaom/BUILD.gn',
    'libavif': 'third_party/libavif/BUILD.gn',
    'libdrm': 'third_party/libdrm/BUILD.gn',
    'libevent': 'third_party/libevent/BUILD.gn',
    'libjpeg': 'third_party/libjpeg.gni',
    'libpng': 'third_party/libpng/BUILD.gn',
    'libsecret': 'third_party/libsecret/BUILD.gn',
    'libusb': 'third_party/libusb/BUILD.gn',
    'libvpx': 'third_party/libvpx/BUILD.gn',
    'libwebp': 'third_party/libwebp/BUILD.gn',
    'libxml': 'third_party/libxml/BUILD.gn',
    'libXNVCtrl': 'third_party/angle/src/third_party/libXNVCtrl/BUILD.gn',
    'libxslt': 'third_party/libxslt/BUILD.gn',
    'libyuv': 'third_party/libyuv/BUILD.gn',
    'openh264': 'third_party/openh264/BUILD.gn',
    'opus': 'third_party/opus/BUILD.gn',
    're2': 'third_party/re2/BUILD.gn',
    'snappy': 'third_party/snappy/BUILD.gn',
    # Use system libSPIRV-Tools in Swiftshader.
    # These two shims MUST be used together.
    'swiftshader-SPIRV-Headers':
    'third_party/swiftshader/third_party/SPIRV-Headers/BUILD.gn',
    'swiftshader-SPIRV-Tools':
    'third_party/swiftshader/third_party/SPIRV-Tools/BUILD.gn',
    # Use system libSPIRV-Tools inside ANGLE.
    # These two shims MUST be used together
    # and can only be used if WebGPU is not compiled (use_dawn=false)
    'vulkan-SPIRV-Headers': 'third_party/spirv-headers/src/BUILD.gn',
    'vulkan-SPIRV-Tools': 'third_party/spirv-tools/src/BUILD.gn',
    #
    'vulkan_memory_allocator': 'third_party/vulkan_memory_allocator/BUILD.gn',
    'woff2': 'third_party/woff2/BUILD.gn',
    'zlib': 'third_party/zlib/BUILD.gn',
    'zstd': 'third_party/zstd/BUILD.gn',
}


def DoMain(argv):
  my_dirname = os.path.dirname(__file__)
  source_tree_root = os.path.abspath(
    os.path.join(my_dirname, '..', '..', '..'))

  parser = argparse.ArgumentParser()
  parser.add_argument('--system-libraries', nargs='*', default=[])
  parser.add_argument('--undo', action='store_true')

  args = parser.parse_args(argv)

  handled_libraries = set()
  for lib, path in REPLACEMENTS.items():
    if lib not in args.system_libraries:
      continue
    handled_libraries.add(lib)

    if args.undo:
      # Restore original file, and also remove the backup.
      # This is meant to restore the source tree to its original state.
      os.rename(os.path.join(source_tree_root, path + '.orig'),
                os.path.join(source_tree_root, path))
    else:
      # Create a backup copy for --undo.
      shutil.copyfile(os.path.join(source_tree_root, path),
                      os.path.join(source_tree_root, path + '.orig'))

      # Copy the GN file from directory of this script to target path.
      shutil.copyfile(os.path.join(my_dirname, '%s.gn' % lib),
                      os.path.join(source_tree_root, path))

  unhandled_libraries = set(args.system_libraries) - handled_libraries
  if unhandled_libraries:
    print('Unrecognized system libraries requested: %s' % ', '.join(
        sorted(unhandled_libraries)), file=sys.stderr)
    return 1

  return 0


if __name__ == '__main__':
  sys.exit(DoMain(sys.argv[1:]))
