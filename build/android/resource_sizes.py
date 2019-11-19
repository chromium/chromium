#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Reports binary size metrics for an APK.

More information at //docs/speed/binary_size/metrics.md.
"""

import argparse
import collections
from contextlib import contextmanager
import json
import logging
import os
import posixpath
import re
import sys
import tempfile
import zipfile
import zlib

from binary_size import apk_downloader
import devil_chromium
from devil.android.sdk import build_tools
from devil.utils import cmd_helper
from devil.utils import lazy
import method_count
from pylib import constants
from pylib.constants import host_paths

_AAPT_PATH = lazy.WeakConstant(lambda: build_tools.GetPath('aapt'))
_BUILD_UTILS_PATH = os.path.join(
    host_paths.DIR_SOURCE_ROOT, 'build', 'android', 'gyp')
_APK_PATCH_SIZE_ESTIMATOR_PATH = os.path.join(
    host_paths.DIR_SOURCE_ROOT, 'third_party', 'apk-patch-size-estimator')

with host_paths.SysPath(host_paths.BUILD_COMMON_PATH):
  import perf_tests_results_helper # pylint: disable=import-error

with host_paths.SysPath(host_paths.TRACING_PATH):
  from tracing.value import convert_chart_json # pylint: disable=import-error

with host_paths.SysPath(_BUILD_UTILS_PATH, 0):
  from util import build_utils # pylint: disable=import-error
  from util import zipalign  # pylint: disable=import-error

with host_paths.SysPath(_APK_PATCH_SIZE_ESTIMATOR_PATH):
  import apk_patch_size_estimator # pylint: disable=import-error


zipalign.ApplyZipFileZipAlignFix()

# Captures an entire config from aapt output.
_AAPT_CONFIG_PATTERN = r'config %s:(.*?)config [a-zA-Z-]+:'
# Matches string resource entries from aapt output.
_AAPT_ENTRY_RE = re.compile(
    r'resource (?P<id>\w{10}) [\w\.]+:string/.*?"(?P<val>.+?)"', re.DOTALL)
_BASE_CHART = {
    'format_version': '0.1',
    'benchmark_name': 'resource_sizes',
    'benchmark_description': 'APK resource size information.',
    'trace_rerun_options': [],
    'charts': {}
}
# Macro definitions look like (something, 123) when
# enable_resource_whitelist_generation=true.
_RC_HEADER_RE = re.compile(r'^#define (?P<name>\w+).* (?P<id>\d+)\)?$')
_RE_NON_LANGUAGE_PAK = re.compile(r'^assets/.*(resources|percent)\.pak$')
_READELF_SIZES_METRICS = {
    'text': ['.text'],
    'data': ['.data', '.rodata', '.data.rel.ro', '.data.rel.ro.local'],
    'relocations': ['.rel.dyn', '.rel.plt', '.rela.dyn', '.rela.plt'],
    'unwind': [
        '.ARM.extab', '.ARM.exidx', '.eh_frame', '.eh_frame_hdr',
        '.ARM.exidxsentinel_section_after_text'
    ],
    'symbols': [
        '.dynsym', '.dynstr', '.dynamic', '.shstrtab', '.got', '.plt',
        '.got.plt', '.hash', '.gnu.hash'
    ],
    'other': [
        '.init_array', '.preinit_array', '.ctors', '.fini_array', '.comment',
        '.note.gnu.gold-version', '.note.crashpad.info', '.note.android.ident',
        '.ARM.attributes', '.note.gnu.build-id', '.gnu.version',
        '.gnu.version_d', '.gnu.version_r', '.interp', '.gcc_except_table'
    ]
}


def _PercentageDifference(a, b):
  if a == 0:
    return 0
  return float(b - a) / a


def _RunReadelf(so_path, options, tool_prefix=''):
  return cmd_helper.GetCmdOutput(
      [tool_prefix + 'readelf'] + options + [so_path])


def _ExtractLibSectionSizesFromApk(apk_path, lib_path, tool_prefix):
  with Unzip(apk_path, filename=lib_path) as extracted_lib_path:
    grouped_section_sizes = collections.defaultdict(int)
    no_bits_section_sizes, section_sizes = _CreateSectionNameSizeMap(
        extracted_lib_path, tool_prefix)
    for group_name, section_names in _READELF_SIZES_METRICS.iteritems():
      for section_name in section_names:
        if section_name in section_sizes:
          grouped_section_sizes[group_name] += section_sizes.pop(section_name)

    # Consider all NOBITS sections as .bss.
    grouped_section_sizes['bss'] = sum(
        v for v in no_bits_section_sizes.itervalues())

    # Group any unknown section headers into the "other" group.
    for section_header, section_size in section_sizes.iteritems():
      sys.stderr.write('Unknown elf section header: %s\n' % section_header)
      grouped_section_sizes['other'] += section_size

    return grouped_section_sizes


def _CreateSectionNameSizeMap(so_path, tool_prefix):
  stdout = _RunReadelf(so_path, ['-S', '--wide'], tool_prefix)
  section_sizes = {}
  no_bits_section_sizes = {}
  # Matches  [ 2] .hash HASH 00000000006681f0 0001f0 003154 04   A  3   0  8
  for match in re.finditer(r'\[[\s\d]+\] (\..*)$', stdout, re.MULTILINE):
    items = match.group(1).split()
    target = no_bits_section_sizes if items[1] == 'NOBITS' else section_sizes
    target[items[0]] = int(items[4], 16)

  return no_bits_section_sizes, section_sizes


def _ParseManifestAttributes(apk_path):
  # Check if the manifest specifies whether or not to extract native libs.
  skip_extract_lib = False
  output = cmd_helper.GetCmdOutput([
      _AAPT_PATH.read(), 'd', 'xmltree', apk_path, 'AndroidManifest.xml'])
  m = re.search(r'extractNativeLibs\(.*\)=\(.*\)(\w)', output)
  if m:
    skip_extract_lib = not bool(int(m.group(1)))

  # Dex decompression overhead varies by Android version.
  m = re.search(r'android:minSdkVersion\(\w+\)=\(type \w+\)(\w+)', output)
  sdk_version = int(m.group(1), 16)

  return sdk_version, skip_extract_lib


def _NormalizeLanguagePaks(translations, factor):
  english_pak = translations.FindByPattern(r'.*/en[-_][Uu][Ss]\.l?pak')
  num_translations = translations.GetNumEntries()
  ret = 0
  if english_pak:
    ret -= translations.ComputeZippedSize()
    ret += int(english_pak.compress_size * num_translations * factor)
  return ret


def _NormalizeResourcesArsc(apk_path, num_arsc_files, num_translations,
                            out_dir):
  """Estimates the expected overhead of untranslated strings in resources.arsc.

  See http://crbug.com/677966 for why this is necessary.
  """
  # If there are multiple .arsc files, use the resource packaged APK instead.
  if num_arsc_files > 1:
    if not out_dir:
      return -float('inf')
    ap_name = os.path.basename(apk_path).replace('.apk', '.ap_')
    ap_path = os.path.join(out_dir, 'arsc/apks', ap_name)
    if not os.path.exists(ap_path):
      raise Exception('Missing expected file: %s, try rebuilding.' % ap_path)
    apk_path = ap_path

  aapt_output = _RunAaptDumpResources(apk_path)
  # en-rUS is in the default config and may be cluttered with non-translatable
  # strings, so en-rGB is a better baseline for finding missing translations.
  en_strings = _CreateResourceIdValueMap(aapt_output, 'en-rGB')
  fr_strings = _CreateResourceIdValueMap(aapt_output, 'fr')

  # en-US and en-GB will never be translated.
  config_count = num_translations - 2

  size = 0
  for res_id, string_val in en_strings.iteritems():
    if string_val == fr_strings[res_id]:
      string_size = len(string_val)
      # 7 bytes is the per-entry overhead (not specific to any string). See
      # https://android.googlesource.com/platform/frameworks/base.git/+/android-4.2.2_r1/tools/aapt/StringPool.cpp#414.
      # The 1.5 factor was determined experimentally and is meant to account for
      # other languages generally having longer strings than english.
      size += config_count * (7 + string_size * 1.5)

  return int(size)


def _CreateResourceIdValueMap(aapt_output, lang):
  """Return a map of resource ids to string values for the given |lang|."""
  config_re = _AAPT_CONFIG_PATTERN % lang
  return {entry.group('id'): entry.group('val')
          for config_section in re.finditer(config_re, aapt_output, re.DOTALL)
          for entry in re.finditer(_AAPT_ENTRY_RE, config_section.group(0))}


def _RunAaptDumpResources(apk_path):
  cmd = [_AAPT_PATH.read(), 'dump', '--values', 'resources', apk_path]
  status, output = cmd_helper.GetCmdStatusAndOutput(cmd)
  if status != 0:
    raise Exception('Failed running aapt command: "%s" with output "%s".' %
                    (' '.join(cmd), output))
  return output


def _ReportDfmSizes(zip_obj, report_func):
  sizes = collections.defaultdict(int)
  for info in zip_obj.infolist():
    # Looks for paths like splits/vr-master.apk, splits/vr-hi.apk.
    name_parts = info.filename.split('/')
    if name_parts[0] == 'splits' and len(name_parts) == 2:
      name_parts = name_parts[1].split('-')
      if len(name_parts) == 2:
        module_name, config_name = name_parts
        if module_name != 'base' and config_name[:-4] in ('master', 'hi'):
          sizes[module_name] += info.file_size

  for module_name, size in sorted(sizes.iteritems()):
    report_func('DFM_' + module_name, 'Size with hindi', size, 'bytes')


class _FileGroup(object):
  """Represents a category that apk files can fall into."""

  def __init__(self, name):
    self.name = name
    self._zip_infos = []
    self._extracted_multipliers = []

  def AddZipInfo(self, zip_info, extracted_multiplier=0):
    self._zip_infos.append(zip_info)
    self._extracted_multipliers.append(extracted_multiplier)

  def AllEntries(self):
    return iter(self._zip_infos)

  def GetNumEntries(self):
    return len(self._zip_infos)

  def FindByPattern(self, pattern):
    return next((i for i in self._zip_infos if re.match(pattern, i.filename)),
                None)

  def FindLargest(self):
    if not self._zip_infos:
      return None
    return max(self._zip_infos, key=lambda i: i.file_size)

  def ComputeZippedSize(self):
    return sum(i.compress_size for i in self._zip_infos)

  def ComputeUncompressedSize(self):
    return sum(i.file_size for i in self._zip_infos)

  def ComputeExtractedSize(self):
    ret = 0
    for zi, multiplier in zip(self._zip_infos, self._extracted_multipliers):
      ret += zi.file_size * multiplier
    return ret

  def ComputeInstallSize(self):
    return self.ComputeExtractedSize() + self.ComputeZippedSize()


def _DoApkAnalysis(apk_filename, apks_path, tool_prefix, out_dir, report_func):
  """Analyse APK to determine size contributions of different file classes."""
  file_groups = []

  def make_group(name):
    group = _FileGroup(name)
    file_groups.append(group)
    return group

  def has_no_extension(filename):
    return os.path.splitext(filename)[1] == ''

  native_code = make_group('Native code')
  java_code = make_group('Java code')
  native_resources_no_translations = make_group('Native resources (no l10n)')
  translations = make_group('Native resources (l10n)')
  stored_translations = make_group('Native resources stored (l10n)')
  icu_data = make_group('ICU (i18n library) data')
  v8_snapshots = make_group('V8 Snapshots')
  png_drawables = make_group('PNG drawables')
  res_directory = make_group('Non-compiled Android resources')
  arsc = make_group('Compiled Android resources')
  metadata = make_group('Package metadata')
  unknown = make_group('Unknown files')
  notices = make_group('licenses.notice file')
  unwind_cfi = make_group('unwind_cfi (dev and canary only)')

  with zipfile.ZipFile(apk_filename, 'r') as apk:
    apk_contents = apk.infolist()

  sdk_version, skip_extract_lib = _ParseManifestAttributes(apk_filename)

  # Pre-L: Dalvik - .odex file is simply decompressed/optimized dex file (~1x).
  # L, M: ART - .odex file is compiled version of the dex file (~4x).
  # N: ART - Uses Dalvik-like JIT for normal apps (~1x), full compilation for
  #    shared apps (~4x).
  # Actual multipliers calculated using "apk_operations.py disk-usage".
  # Will need to update multipliers once apk obfuscation is enabled.
  # E.g. with obfuscation, the 4.04 changes to 4.46.
  speed_profile_dex_multiplier = 1.17
  orig_filename = apks_path or apk_filename
  is_monochrome = 'Monochrome' in orig_filename
  is_webview = 'WebView' in orig_filename
  is_shared_apk = sdk_version >= 24 and (is_monochrome or is_webview)
  if sdk_version < 21:
    # JellyBean & KitKat
    dex_multiplier = 1.16
  elif sdk_version < 24:
    # Lollipop & Marshmallow
    dex_multiplier = 4.04
  elif is_shared_apk:
    # Oreo and above, compilation_filter=speed
    dex_multiplier = 4.04
  else:
    # Oreo and above, compilation_filter=speed-profile
    dex_multiplier = speed_profile_dex_multiplier

  total_apk_size = os.path.getsize(apk_filename)
  for member in apk_contents:
    filename = member.filename
    if filename.endswith('/'):
      continue
    if filename.endswith('.so'):
      basename = posixpath.basename(filename)
      should_extract_lib = not skip_extract_lib and basename.startswith('lib')
      native_code.AddZipInfo(
          member, extracted_multiplier=int(should_extract_lib))
    elif filename.endswith('.dex'):
      java_code.AddZipInfo(member, extracted_multiplier=dex_multiplier)
    elif re.search(_RE_NON_LANGUAGE_PAK, filename):
      native_resources_no_translations.AddZipInfo(member)
    elif filename.endswith('.pak') or filename.endswith('.lpak'):
      compressed = member.compress_type != zipfile.ZIP_STORED
      bucket = translations if compressed else stored_translations
      extracted_multiplier = 0
      if compressed:
        extracted_multiplier = int('en_' in filename or 'en-' in filename)
      bucket.AddZipInfo(member, extracted_multiplier=extracted_multiplier)
    elif filename == 'assets/icudtl.dat':
      icu_data.AddZipInfo(member)
    elif filename.endswith('.bin'):
      v8_snapshots.AddZipInfo(member)
    elif filename.startswith('res/'):
      if (filename.endswith('.png') or filename.endswith('.webp')
          or has_no_extension(filename)):
        png_drawables.AddZipInfo(member)
      else:
        res_directory.AddZipInfo(member)
    elif filename.endswith('.arsc'):
      arsc.AddZipInfo(member)
    elif filename.startswith('META-INF') or filename == 'AndroidManifest.xml':
      metadata.AddZipInfo(member)
    elif filename.endswith('.notice'):
      notices.AddZipInfo(member)
    elif filename.startswith('assets/unwind_cfi'):
      unwind_cfi.AddZipInfo(member)
    else:
      unknown.AddZipInfo(member)

  if apks_path:
    # We're mostly focused on size of Chrome for non-English locales, so assume
    # Hindi (arbitrarily chosen) locale split is installed.
    with zipfile.ZipFile(apks_path) as z:
      hindi_apk_info = z.getinfo('splits/base-hi.apk')
      total_apk_size += hindi_apk_info.file_size
      _ReportDfmSizes(z, report_func)

  total_install_size = total_apk_size
  total_install_size_android_go = total_apk_size
  zip_overhead = total_apk_size

  for group in file_groups:
    actual_size = group.ComputeZippedSize()
    install_size = group.ComputeInstallSize()
    uncompressed_size = group.ComputeUncompressedSize()
    extracted_size = group.ComputeExtractedSize()
    total_install_size += extracted_size
    zip_overhead -= actual_size

    report_func('Breakdown', group.name + ' size', actual_size, 'bytes')
    report_func('InstallBreakdown', group.name + ' size', int(install_size),
                'bytes')
    # Only a few metrics are compressed in the first place.
    # To avoid over-reporting, track uncompressed size only for compressed
    # entries.
    if uncompressed_size != actual_size:
      report_func('Uncompressed', group.name + ' size', uncompressed_size,
                  'bytes')

    if group is java_code and is_shared_apk:
      # Updates are compiled using quicken, but system image uses speed-profile.
      extracted_size = int(uncompressed_size * speed_profile_dex_multiplier)
      total_install_size_android_go += extracted_size
      report_func('InstallBreakdownGo', group.name + ' size',
                  actual_size + extracted_size, 'bytes')
    elif group is translations and apks_path:
      # Assume Hindi rather than English (accounted for above in total_apk_size)
      total_install_size_android_go += actual_size
    else:
      total_install_size_android_go += extracted_size

  # Per-file zip overhead is caused by:
  # * 30 byte entry header + len(file name)
  # * 46 byte central directory entry + len(file name)
  # * 0-3 bytes for zipalign.
  report_func('Breakdown', 'Zip Overhead', zip_overhead, 'bytes')
  report_func('InstallSize', 'APK size', total_apk_size, 'bytes')
  report_func('InstallSize', 'Estimated installed size',
              int(total_install_size), 'bytes')
  if is_shared_apk:
    report_func('InstallSize', 'Estimated installed size (Android Go)',
                int(total_install_size_android_go), 'bytes')
  transfer_size = _CalculateCompressedSize(apk_filename)
  report_func('TransferSize', 'Transfer size (deflate)', transfer_size, 'bytes')

  # Size of main dex vs remaining.
  main_dex_info = java_code.FindByPattern('classes.dex')
  if main_dex_info:
    main_dex_size = main_dex_info.file_size
    report_func('Specifics', 'main dex size', main_dex_size, 'bytes')
    secondary_size = java_code.ComputeUncompressedSize() - main_dex_size
    report_func('Specifics', 'secondary dex size', secondary_size, 'bytes')

  main_lib_info = native_code.FindLargest()
  native_code_unaligned_size = 0
  for lib_info in native_code.AllEntries():
    section_sizes = _ExtractLibSectionSizesFromApk(
        apk_filename, lib_info.filename, tool_prefix)
    native_code_unaligned_size += sum(
        v for k, v in section_sizes.iteritems() if k != 'bss')
    # Size of main .so vs remaining.
    if lib_info == main_lib_info:
      main_lib_size = lib_info.file_size
      report_func('Specifics', 'main lib size', main_lib_size, 'bytes')
      secondary_size = native_code.ComputeUncompressedSize() - main_lib_size
      report_func('Specifics', 'other lib size', secondary_size, 'bytes')

      for metric_name, size in section_sizes.iteritems():
        report_func('MainLibInfo', metric_name, size, 'bytes')

  # Main metric that we want to monitor for jumps.
  normalized_apk_size = total_apk_size
  # unwind_cfi exists only in dev, canary, and non-channel builds.
  normalized_apk_size -= unwind_cfi.ComputeZippedSize()
  # Sections within .so files get 4kb aligned, so use section sizes rather than
  # file size. Also gets rid of compression.
  normalized_apk_size -= native_code.ComputeZippedSize()
  normalized_apk_size += native_code_unaligned_size
  # Normalized dex size: Size within the zip + size on disk for Android Go
  # devices running Android O (which ~= uncompressed dex size).
  # Use a constant compression factor to account for fluctuations.
  normalized_apk_size -= java_code.ComputeZippedSize()
  normalized_apk_size += java_code.ComputeUncompressedSize()
  # Unaligned size should be ~= uncompressed size or something is wrong.
  # As of now, padding_fraction ~= .007
  padding_fraction = -_PercentageDifference(
      native_code.ComputeUncompressedSize(), native_code_unaligned_size)
  assert 0 <= padding_fraction < .02, (
      'Padding was: {} (file_size={}, sections_sum={})'.format(
          padding_fraction, native_code.ComputeUncompressedSize(),
          native_code_unaligned_size))

  if apks_path:
    # Locale normalization not needed when measuring only one locale.
    # E.g. a change that adds 300 chars of unstranslated strings would cause the
    # metric to be off by only 390 bytes (assuming a multiplier of 2.3 for
    # Hindi).
    pass
  else:
    # Avoid noise caused when strings change and translations haven't yet been
    # updated.
    num_translations = translations.GetNumEntries()
    num_stored_translations = stored_translations.GetNumEntries()

    if num_translations > 1:
      # Multipliers found by looking at MonochromePublic.apk and seeing how much
      # smaller en-US.pak is relative to the average locale.pak.
      normalized_apk_size += _NormalizeLanguagePaks(translations, 1.17)
    if num_stored_translations > 1:
      normalized_apk_size += _NormalizeLanguagePaks(stored_translations, 1.43)
    if num_translations + num_stored_translations > 1:
      if num_translations == 0:
        # WebView stores all locale paks uncompressed.
        num_arsc_translations = num_stored_translations
      else:
        # Monochrome has more configurations than Chrome since it includes
        # WebView (which supports more locales), but these should mostly be
        # empty so ignore them here.
        num_arsc_translations = num_translations
      normalized_apk_size += _NormalizeResourcesArsc(
          apk_filename, arsc.GetNumEntries(), num_arsc_translations, out_dir)

  # It will be -Inf for .apk files with multiple .arsc files and no out_dir set.
  if normalized_apk_size < 0:
    sys.stderr.write('Skipping normalized_apk_size (no output directory set)\n')
  else:
    report_func('Specifics', 'normalized apk size', normalized_apk_size,
                'bytes')
  # The "file count" metric cannot be grouped with any other metrics when the
  # end result is going to be uploaded to the perf dashboard in the HistogramSet
  # format due to mixed units (bytes vs. zip entries) causing malformed
  # summaries to be generated.
  # TODO(https://crbug.com/903970): Remove this workaround if unit mixing is
  # ever supported.
  report_func('FileCount', 'file count', len(apk_contents), 'zip entries')

  for info in unknown.AllEntries():
    sys.stderr.write(
        'Unknown entry: %s %d\n' % (info.filename, info.compress_size))


def _CalculateCompressedSize(file_path):
  CHUNK_SIZE = 256 * 1024
  compressor = zlib.compressobj()
  total_size = 0
  with open(file_path, 'rb') as f:
    for chunk in iter(lambda: f.read(CHUNK_SIZE), ''):
      total_size += len(compressor.compress(chunk))
  total_size += len(compressor.flush())
  return total_size


def _DoDexAnalysis(apk_filename, report_func):
  sizes, total_size, num_unique_methods = method_count.ExtractSizesFromZip(
      apk_filename)
  cumulative_sizes = collections.defaultdict(int)
  for classes_dex_sizes in sizes.itervalues():
    for count_type, count in classes_dex_sizes.iteritems():
      cumulative_sizes[count_type] += count
  for count_type, count in cumulative_sizes.iteritems():
    report_func('Dex', count_type, count, 'entries')

  report_func('Dex', 'unique methods', num_unique_methods, 'entries')
  report_func('DexCache', 'DexCache', total_size, 'bytes')


def _PrintPatchSizeEstimate(new_apk, builder, bucket, report_func):
  apk_name = os.path.basename(new_apk)
  # Reference APK paths have spaces replaced by underscores.
  builder = builder.replace(' ', '_')
  old_apk = apk_downloader.MaybeDownloadApk(
      builder, apk_downloader.CURRENT_MILESTONE, apk_name,
      apk_downloader.DEFAULT_DOWNLOAD_PATH, bucket)
  if old_apk:
    # Use a temp dir in case patch size functions fail to clean up temp files.
    with build_utils.TempDir() as tmp:
      tmp_name = os.path.join(tmp, 'patch.tmp')
      bsdiff = apk_patch_size_estimator.calculate_bsdiff(
          old_apk, new_apk, None, tmp_name)
      report_func('PatchSizeEstimate', 'BSDiff (gzipped)', bsdiff, 'bytes')
      fbf = apk_patch_size_estimator.calculate_filebyfile(
          old_apk, new_apk, None, tmp_name)
      report_func('PatchSizeEstimate', 'FileByFile (gzipped)', fbf, 'bytes')


@contextmanager
def Unzip(zip_file, filename=None):
  """Utility for temporary use of a single file in a zip archive."""
  with build_utils.TempDir() as unzipped_dir:
    unzipped_files = build_utils.ExtractAll(
        zip_file, unzipped_dir, True, pattern=filename)
    if len(unzipped_files) == 0:
      raise Exception(
          '%s not found in %s' % (filename, zip_file))
    yield unzipped_files[0]


def _ConfigOutDirAndToolsPrefix(out_dir):
  if out_dir:
    constants.SetOutputDirectory(out_dir)
  else:
    try:
      # Triggers auto-detection when CWD == output directory.
      constants.CheckOutputDirectory()
      out_dir = constants.GetOutDirectory()
    except Exception:  # pylint: disable=broad-except
      return out_dir, ''
  build_vars = build_utils.ReadBuildVars(
      os.path.join(out_dir, "build_vars.txt"))
  tool_prefix = os.path.join(out_dir, build_vars['android_tool_prefix'])
  return out_dir, tool_prefix


def _Analyze(apk_path, chartjson, args):

  def report_func(*args):
    # Do not add any new metrics without also documenting them in:
    # //docs/speed/binary_size/metrics.md.
    perf_tests_results_helper.ReportPerfResult(chartjson, *args)

  out_dir, tool_prefix = _ConfigOutDirAndToolsPrefix(args.out_dir)
  apks_path = args.input if args.input.endswith('.apks') else None
  _DoApkAnalysis(apk_path, apks_path, tool_prefix, out_dir, report_func)
  _DoDexAnalysis(apk_path, report_func)
  if args.estimate_patch_size:
    _PrintPatchSizeEstimate(apk_path, args.reference_apk_builder,
                            args.reference_apk_bucket, report_func)


def ResourceSizes(args):
  chartjson = _BASE_CHART.copy() if args.output_format else None

  if args.input.endswith('.apk'):
    _Analyze(args.input, chartjson, args)
  elif args.input.endswith('.apks'):
    with tempfile.NamedTemporaryFile(suffix='.apk') as f:
      with zipfile.ZipFile(args.input) as z:
        # Currently bundletool is creating two apks when .apks is created
        # without specifying an sdkVersion. Always measure the one with an
        # uncompressed shared library.
        try:
          info = z.getinfo('splits/base-master_2.apk')
        except KeyError:
          info = z.getinfo('splits/base-master.apk')
        f.write(z.read(info))
        f.flush()
      _Analyze(f.name, chartjson, args)
  else:
    raise Exception('Unknown file type: ' + args.input)

  if chartjson:
    if args.output_file == '-':
      json_file = sys.stdout
    elif args.output_file:
      json_file = open(args.output_file, 'w')
    else:
      results_path = os.path.join(args.output_dir, 'results-chart.json')
      logging.critical('Dumping chartjson to %s', results_path)
      json_file = open(results_path, 'w')

    json.dump(chartjson, json_file, indent=2)

    if json_file is not sys.stdout:
      json_file.close()

    # We would ideally generate a histogram set directly instead of generating
    # chartjson then converting. However, perf_tests_results_helper is in
    # //build, which doesn't seem to have any precedent for depending on
    # anything in Catapult. This can probably be fixed, but since this doesn't
    # need to be super fast or anything, converting is a good enough solution
    # for the time being.
    if args.output_format == 'histograms':
      histogram_result = convert_chart_json.ConvertChartJson(results_path)
      if histogram_result.returncode != 0:
        logging.error('chartjson conversion failed with error: %s',
            histogram_result.stdout)
        return 1

      histogram_path = os.path.join(args.output_dir, 'perf_results.json')
      logging.critical('Dumping histograms to %s', histogram_path)
      with open(histogram_path, 'w') as json_file:
        json_file.write(histogram_result.stdout)

  return 0


def main():
  argparser = argparse.ArgumentParser(description='Print APK size metrics.')
  argparser.add_argument(
      '--min-pak-resource-size',
      type=int,
      default=20 * 1024,
      help='Minimum byte size of displayed pak resources.')
  argparser.add_argument(
      '--chromium-output-directory',
      dest='out_dir',
      type=os.path.realpath,
      help='Location of the build artifacts.')
  argparser.add_argument(
      '--chartjson',
      action='store_true',
      help='DEPRECATED. Use --output-format=chartjson '
      'instead.')
  argparser.add_argument(
      '--output-format',
      choices=['chartjson', 'histograms'],
      help='Output the results to a file in the given '
      'format instead of printing the results.')
  argparser.add_argument('--loadable_module', help='Obsolete (ignored).')
  argparser.add_argument(
      '--estimate-patch-size',
      action='store_true',
      help='Include patch size estimates. Useful for perf '
      'builders where a reference APK is available but adds '
      '~3 mins to run time.')
  argparser.add_argument(
      '--reference-apk-builder',
      default=apk_downloader.DEFAULT_BUILDER,
      help='Builder name to use for reference APK for patch '
      'size estimates.')
  argparser.add_argument(
      '--reference-apk-bucket',
      default=apk_downloader.DEFAULT_BUCKET,
      help='Storage bucket holding reference APKs.')

  # Accepted to conform to the isolated script interface, but ignored.
  argparser.add_argument(
      '--isolated-script-test-filter', help=argparse.SUPPRESS)
  argparser.add_argument(
      '--isolated-script-test-perf-output',
      type=os.path.realpath,
      help=argparse.SUPPRESS)

  output_group = argparser.add_mutually_exclusive_group()

  output_group.add_argument(
      '--output-dir', default='.', help='Directory to save chartjson to.')
  output_group.add_argument(
      '--output-file',
      help='Path to output .json (replaces --output-dir). Works only for '
      '--output-format=chartjson')
  output_group.add_argument(
      '--isolated-script-test-output',
      type=os.path.realpath,
      help='File to which results will be written in the '
      'simplified JSON output format.')

  argparser.add_argument('input', help='Path to .apk or .apks file to measure.')
  args = argparser.parse_args()

  devil_chromium.Initialize(output_directory=args.out_dir)

  # TODO(bsheedy): Remove this once uses of --chartjson have been removed.
  if args.chartjson:
    args.output_format = 'chartjson'

  isolated_script_output = {'valid': False, 'failures': []}

  test_name = 'resource_sizes (%s)' % os.path.basename(args.input)

  if args.isolated_script_test_output:
    args.output_dir = os.path.join(
        os.path.dirname(args.isolated_script_test_output), test_name)
    if not os.path.exists(args.output_dir):
      os.makedirs(args.output_dir)

  try:
    result = ResourceSizes(args)
    isolated_script_output = {
        'valid': True,
        'failures': [test_name] if result else [],
    }
  finally:
    if args.isolated_script_test_output:
      results_path = os.path.join(args.output_dir, 'test_results.json')
      with open(results_path, 'w') as output_file:
        json.dump(isolated_script_output, output_file)
      with open(args.isolated_script_test_output, 'w') as output_file:
        json.dump(isolated_script_output, output_file)

  return result


if __name__ == '__main__':
  sys.exit(main())
