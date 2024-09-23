#!/usr/bin/env vpython3
# Copyright 2011 The Chromium Authors
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
import struct
import sys
import tempfile
import zipfile
import zlib

import devil_chromium
from devil.android.sdk import build_tools
from devil.utils import cmd_helper
from devil.utils import lazy
import method_count
from pylib import constants
from pylib.constants import host_paths

_AAPT_PATH = lazy.WeakConstant(lambda: build_tools.GetPath('aapt'))
_ANDROID_UTILS_PATH = os.path.join(host_paths.DIR_SOURCE_ROOT, 'build',
                                   'android', 'gyp')
_READOBJ_PATH = os.path.join(host_paths.DIR_SOURCE_ROOT, 'third_party',
                             'llvm-build', 'Release+Asserts', 'bin',
                             'llvm-readobj')

with host_paths.SysPath(host_paths.BUILD_UTIL_PATH):
  from lib.common import perf_tests_results_helper
  from lib.results import result_sink
  from lib.results import result_types

with host_paths.SysPath(host_paths.TRACING_PATH):
  from tracing.value import convert_chart_json  # pylint: disable=import-error

with host_paths.SysPath(_ANDROID_UTILS_PATH, 0):
  from util import build_utils  # pylint: disable=import-error

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
# enable_resource_allowlist_generation=true.
_RC_HEADER_RE = re.compile(r'^#define (?P<name>\w+).* (?P<id>\d+)\)?$')
_RE_NON_LANGUAGE_PAK = re.compile(r'^assets/.*(resources|percent)\.pak$')
_READELF_SIZES_METRICS = {
    'text': ['.text'],
    'data': ['.data', '.rodata', '.data.rel.ro', '.data.rel.ro.local'],
    'relocations':
    ['.rel.dyn', '.rel.plt', '.rela.dyn', '.rela.plt', '.relr.dyn'],
    'unwind': [
        '.ARM.extab', '.ARM.exidx', '.eh_frame', '.eh_frame_hdr',
        '.ARM.exidxsentinel_section_after_text'
    ],
    'symbols': [
        '.dynsym', '.dynstr', '.dynamic', '.shstrtab', '.got', '.plt', '.iplt',
        '.got.plt', '.hash', '.gnu.hash'
    ],
    'other': [
        '.init_array', '.preinit_array', '.ctors', '.fini_array', '.comment',
        '.note.gnu.gold-version', '.note.crashpad.info', '.note.android.ident',
        '.ARM.attributes', '.note.gnu.build-id', '.gnu.version',
        '.gnu.version_d', '.gnu.version_r', '.interp', '.gcc_except_table',
        '.note.gnu.property'
    ]
}


class _AccumulatingReporter:
  def __init__(self):
    self._combined_metrics = collections.defaultdict(int)

  def __call__(self, graph_title, trace_title, value, units):
    self._combined_metrics[(graph_title, trace_title, units)] += value

  def DumpReports(self, report_func):
    for (graph_title, trace_title,
         units), value in sorted(self._combined_metrics.items()):
      report_func(graph_title, trace_title, value, units)


class _ChartJsonReporter(_AccumulatingReporter):
  def __init__(self, chartjson):
    super().__init__()
    self._chartjson = chartjson
    self.trace_title_prefix = ''

  def __call__(self, graph_title, trace_title, value, units):
    super().__call__(graph_title, trace_title, value, units)

    perf_tests_results_helper.ReportPerfResult(
        self._chartjson, graph_title, self.trace_title_prefix + trace_title,
        value, units)

  def SynthesizeTotals(self, unique_method_count):
    for tup, value in sorted(self._combined_metrics.items()):
      graph_title, trace_title, units = tup
      if trace_title == 'unique methods':
        value = unique_method_count
      perf_tests_results_helper.ReportPerfResult(self._chartjson, graph_title,
                                                 'Combined_' + trace_title,
                                                 value, units)


def _PercentageDifference(a, b):
  if a == 0:
    return 0
  return float(b - a) / a


def _ReadZipInfoExtraFieldLength(zip_file, zip_info):
  """Reads the value of |extraLength| from |zip_info|'s local file header.

  |zip_info| has an |extra| field, but it's read from the central directory.
  Android's zipalign tool sets the extra field only in local file headers.
  """
  # Refer to https://en.wikipedia.org/wiki/Zip_(file_format)#File_headers
  zip_file.fp.seek(zip_info.header_offset + 28)
  return struct.unpack('<H', zip_file.fp.read(2))[0]


def _MeasureApkSignatureBlock(zip_file):
  """Measures the size of the v2 / v3 signing block.

  Refer to: https://source.android.com/security/apksigning/v2
  """
  # Seek to "end of central directory" struct.
  eocd_offset_from_end = -22 - len(zip_file.comment)
  zip_file.fp.seek(eocd_offset_from_end, os.SEEK_END)
  assert zip_file.fp.read(4) == b'PK\005\006', (
      'failed to find end-of-central-directory')

  # Read out the "start of central directory" offset.
  zip_file.fp.seek(eocd_offset_from_end + 16, os.SEEK_END)
  start_of_central_directory = struct.unpack('<I', zip_file.fp.read(4))[0]

  # Compute the offset after the last zip entry.
  last_info = max(zip_file.infolist(), key=lambda i: i.header_offset)
  last_header_size = (30 + len(last_info.filename) +
                      _ReadZipInfoExtraFieldLength(zip_file, last_info))
  end_of_last_file = (last_info.header_offset + last_header_size +
                      last_info.compress_size)
  return start_of_central_directory - end_of_last_file


def _RunReadobj(so_path, options):
  return cmd_helper.GetCmdOutput([_READOBJ_PATH, '--elf-output-style=GNU'] +
                                 options + [so_path])


def _ExtractLibSectionSizesFromApk(apk_path, lib_path):
  with Unzip(apk_path, filename=lib_path) as extracted_lib_path:
    grouped_section_sizes = collections.defaultdict(int)
    no_bits_section_sizes, section_sizes = _CreateSectionNameSizeMap(
        extracted_lib_path)
    for group_name, section_names in _READELF_SIZES_METRICS.items():
      for section_name in section_names:
        if section_name in section_sizes:
          grouped_section_sizes[group_name] += section_sizes.pop(section_name)

    # Consider all NOBITS sections as .bss.
    grouped_section_sizes['bss'] = sum(no_bits_section_sizes.values())

    # Group any unknown section headers into the "other" group.
    for section_header, section_size in section_sizes.items():
      sys.stderr.write('Unknown elf section header: %s\n' % section_header)
      grouped_section_sizes['other'] += section_size

    return grouped_section_sizes


def _CreateSectionNameSizeMap(so_path):
  stdout = _RunReadobj(so_path, ['-S', '--wide'])
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
  output = cmd_helper.GetCmdOutput([
      _AAPT_PATH.read(), 'd', 'xmltree', apk_path, 'AndroidManifest.xml'])

  def parse_attr(namespace, name, default=None):
    # android:extractNativeLibs(0x010104ea)=(type 0x12)0x0
    # android:extractNativeLibs(0x010104ea)=(type 0x12)0xffffffff
    # dist:onDemand=(type 0x12)0xffffffff
    m = re.search(
        f'(?:{namespace}:)?{name}' + r'(?:\(.*?\))?=\(type .*?\)(\w+)', output)
    if m is None:
      return default
    return int(m.group(1), 16)

  skip_extract_lib = not parse_attr('android', 'extractNativeLibs', default=1)
  sdk_version = parse_attr('android', 'minSdkVersion')
  is_feature_split = parse_attr('android', 'isFeatureSplit')
  # Can use <dist:on-demand>, or <module dist:onDemand="true">.
  on_demand = parse_attr('dist', 'onDemand') or 'on-demand' in output
  on_demand = bool(on_demand and is_feature_split)

  return sdk_version, skip_extract_lib, on_demand


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
  for res_id, string_val in en_strings.items():
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


class _FileGroup:
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


def _AnalyzeInternal(apk_path,
                     sdk_version,
                     report_func,
                     dex_stats_collector,
                     out_dir,
                     apks_path=None,
                     split_name=None):
  """Analyse APK to determine size contributions of different file classes.

  Returns: Normalized APK size.
  """
  dex_stats_collector.CollectFromZip(split_name or '', apk_path)
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
  notices = make_group('licenses.notice file')
  unwind_cfi = make_group('unwind_cfi (dev and canary only)')
  assets = make_group('Other Android Assets')
  unknown = make_group('Unknown files')

  with zipfile.ZipFile(apk_path, 'r') as apk:
    apk_contents = apk.infolist()
    # Account for zipalign overhead that exists in local file header.
    zipalign_overhead = sum(
        _ReadZipInfoExtraFieldLength(apk, i) for i in apk_contents)
    # Account for zipalign overhead that exists in central directory header.
    # Happens when python aligns entries in apkbuilder.py, but does not
    # exist when using Android's zipalign. E.g. for bundle .apks files.
    zipalign_overhead += sum(len(i.extra) for i in apk_contents)
    signing_block_size = _MeasureApkSignatureBlock(apk)

  _, skip_extract_lib, _ = _ParseManifestAttributes(apk_path)

  # Pre-L: Dalvik - .odex file is simply decompressed/optimized dex file (~1x).
  # L, M: ART - .odex file is compiled version of the dex file (~4x).
  # N: ART - Uses Dalvik-like JIT for normal apps (~1x), full compilation for
  #    shared apps (~4x).
  # Actual multipliers calculated using "apk_operations.py disk-usage".
  # Will need to update multipliers once apk obfuscation is enabled.
  # E.g. with obfuscation, the 4.04 changes to 4.46.
  speed_profile_dex_multiplier = 1.17
  orig_filename = apks_path or apk_path
  is_webview = 'WebView' in orig_filename
  is_monochrome = 'Monochrome' in orig_filename
  is_library = 'Library' in orig_filename
  is_trichrome = 'TrichromeChrome' in orig_filename
  # WebView is always a shared APK since other apps load it.
  # Library is always shared since it's used by chrome and webview
  # Chrome is always shared since renderers can't access dex otherwise
  # (see DexFixer).
  is_shared_apk = sdk_version >= 24 and (is_monochrome or is_webview
                                         or is_library or is_trichrome)
  # Dex decompression overhead varies by Android version.
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

  total_apk_size = os.path.getsize(apk_path)
  for member in apk_contents:
    filename = member.filename
    # Undo asset path suffixing. https://crbug.com/357131361
    if filename.endswith('+'):
      suffix_idx = filename.rfind('+', 0, len(filename) - 1)
      if suffix_idx != -1:
        filename = filename[:suffix_idx]

    if filename.endswith('/'):
      continue
    if filename.endswith('.so'):
      basename = posixpath.basename(filename)
      should_extract_lib = not skip_extract_lib and basename.startswith('lib')
      native_code.AddZipInfo(
          member, extracted_multiplier=int(should_extract_lib))
    elif filename.startswith('classes') and filename.endswith('.dex'):
      # Android P+, uncompressed dex does not need to be extracted.
      compressed = member.compress_type != zipfile.ZIP_STORED
      multiplier = dex_multiplier
      if not compressed and sdk_version >= 28:
        multiplier -= 1

      java_code.AddZipInfo(member, extracted_multiplier=multiplier)
    elif re.search(_RE_NON_LANGUAGE_PAK, filename):
      native_resources_no_translations.AddZipInfo(member)
    elif filename.endswith('.pak') or filename.endswith('.lpak'):
      compressed = member.compress_type != zipfile.ZIP_STORED
      bucket = translations if compressed else stored_translations
      extracted_multiplier = 0
      if compressed:
        extracted_multiplier = int('en_' in filename or 'en-' in filename)
      bucket.AddZipInfo(member, extracted_multiplier=extracted_multiplier)
    elif 'icu' in filename and filename.endswith('.dat'):
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
    elif filename.startswith('META-INF') or filename in (
        'AndroidManifest.xml', 'assets/webapk_dex_version.txt',
        'stamp-cert-sha256'):
      metadata.AddZipInfo(member)
    elif filename.endswith('.notice'):
      notices.AddZipInfo(member)
    elif filename.startswith('assets/unwind_cfi'):
      unwind_cfi.AddZipInfo(member)
    elif filename.startswith('assets/'):
      assets.AddZipInfo(member)
    else:
      unknown.AddZipInfo(member)

  if apks_path:
    # We're mostly focused on size of Chrome for non-English locales, so assume
    # Hindi (arbitrarily chosen) locale split is installed.
    with zipfile.ZipFile(apks_path) as z:
      subpath = 'splits/{}-hi.apk'.format(split_name)
      if subpath in z.namelist():
        hindi_apk_info = z.getinfo(subpath)
        total_apk_size += hindi_apk_info.file_size
      else:
        assert split_name != 'base', 'splits/base-hi.apk should always exist'

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

    if group is java_code:
      # Updates are compiled using quicken, but system image uses speed-profile.
      multiplier = speed_profile_dex_multiplier

      # Android P+, uncompressed dex does not need to be extracted.
      compressed = uncompressed_size != actual_size
      if not compressed and sdk_version >= 28:
        multiplier -= 1
      extracted_size = int(uncompressed_size * multiplier)
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
  report_func('InstallSize', 'Estimated installed size (Android Go)',
              int(total_install_size_android_go), 'bytes')
  transfer_size = _CalculateCompressedSize(apk_path)
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
    # Skip placeholders.
    if lib_info.file_size == 0:
      continue
    section_sizes = _ExtractLibSectionSizesFromApk(apk_path, lib_info.filename)
    native_code_unaligned_size += sum(v for k, v in section_sizes.items()
                                      if k != 'bss')
    # Size of main .so vs remaining.
    if lib_info == main_lib_info:
      main_lib_size = lib_info.file_size
      report_func('Specifics', 'main lib size', main_lib_size, 'bytes')
      secondary_size = native_code.ComputeUncompressedSize() - main_lib_size
      report_func('Specifics', 'other lib size', secondary_size, 'bytes')

      for metric_name, size in section_sizes.items():
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
  # Don't include zipalign overhead in normalized size, since it effectively
  # causes size changes files that proceed aligned files to be rounded.
  # For APKs where classes.dex directly proceeds libchrome.so (the normal case),
  # this causes small dex size changes to disappear into libchrome.so alignment.
  normalized_apk_size -= zipalign_overhead
  # Don't include the size of the apk's signing block because it can fluctuate
  # by up to 4kb (from my non-scientific observations), presumably based on hash
  # sizes.
  normalized_apk_size -= signing_block_size

  # Unaligned size should be ~= uncompressed size or something is wrong.
  # As of now, padding_fraction ~= .007
  padding_fraction = -_PercentageDifference(
      native_code.ComputeUncompressedSize(), native_code_unaligned_size)
  # Ignore this check for small / no native code
  if native_code.ComputeUncompressedSize() > 1000000:
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
      normalized_apk_size += _NormalizeResourcesArsc(apk_path,
                                                     arsc.GetNumEntries(),
                                                     num_arsc_translations,
                                                     out_dir)

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
  # TODO(crbug.com/41425646): Remove this workaround if unit mixing is
  # ever supported.
  report_func('FileCount', 'file count', len(apk_contents), 'zip entries')

  for info in unknown.AllEntries():
    sys.stderr.write(
        'Unknown entry: %s %d\n' % (info.filename, info.compress_size))
  return normalized_apk_size


def _CalculateCompressedSize(file_path):
  CHUNK_SIZE = 256 * 1024
  compressor = zlib.compressobj()
  total_size = 0
  with open(file_path, 'rb') as f:
    for chunk in iter(lambda: f.read(CHUNK_SIZE), b''):
      total_size += len(compressor.compress(chunk))
  total_size += len(compressor.flush())
  return total_size


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


def _ConfigOutDir(out_dir):
  if out_dir:
    constants.SetOutputDirectory(out_dir)
  else:
    try:
      # Triggers auto-detection when CWD == output directory.
      constants.CheckOutputDirectory()
      out_dir = constants.GetOutDirectory()
    except Exception:  # pylint: disable=broad-except
      pass
  return out_dir


def _IterSplits(namelist):
  for subpath in namelist:
    # Looks for paths like splits/vr-master.apk, splits/vr-hi.apk.
    name_parts = subpath.split('/')
    if name_parts[0] == 'splits' and len(name_parts) == 2:
      name_parts = name_parts[1].split('-')
      if len(name_parts) == 2:
        split_name, config_name = name_parts
        if config_name == 'master.apk':
          yield subpath, split_name


def _ExtractToTempFile(zip_obj, subpath, temp_file):
  temp_file.seek(0)
  temp_file.truncate()
  temp_file.write(zip_obj.read(subpath))
  temp_file.flush()


def _AnalyzeApkOrApks(report_func, apk_path, out_dir):
  # Create DexStatsCollector here to track unique methods across base & chrome
  # modules.
  dex_stats_collector = method_count.DexStatsCollector()

  if apk_path.endswith('.apk'):
    sdk_version, _, _ = _ParseManifestAttributes(apk_path)
    _AnalyzeInternal(apk_path, sdk_version, report_func, dex_stats_collector,
                     out_dir)
  elif apk_path.endswith('.apks'):
    with tempfile.NamedTemporaryFile(suffix='.apk') as f:
      with zipfile.ZipFile(apk_path) as z:
        # Currently bundletool is creating two apks when .apks is created
        # without specifying an sdkVersion. Always measure the one with an
        # uncompressed shared library.
        try:
          info = z.getinfo('splits/base-master_2.apk')
        except KeyError:
          info = z.getinfo('splits/base-master.apk')
        _ExtractToTempFile(z, info.filename, f)
        sdk_version, _, _ = _ParseManifestAttributes(f.name)

        orig_report_func = report_func
        report_func = _AccumulatingReporter()

        def do_measure(split_name, on_demand):
          logging.info('Measuring %s on_demand=%s', split_name, on_demand)
          # Use no-op reporting functions to get normalized size for DFMs.
          inner_report_func = report_func
          inner_dex_stats_collector = dex_stats_collector
          if on_demand:
            inner_report_func = lambda *_: None
            inner_dex_stats_collector = method_count.DexStatsCollector()

          size = _AnalyzeInternal(f.name,
                                  sdk_version,
                                  inner_report_func,
                                  inner_dex_stats_collector,
                                  out_dir,
                                  apks_path=apk_path,
                                  split_name=split_name)
          report_func('DFM_' + split_name, 'Size with hindi', size, 'bytes')

        # Measure base outside of the loop since we've already extracted it.
        do_measure('base', on_demand=False)

        for subpath, split_name in _IterSplits(z.namelist()):
          if split_name != 'base':
            _ExtractToTempFile(z, subpath, f)
            _, _, on_demand = _ParseManifestAttributes(f.name)
            do_measure(split_name, on_demand=on_demand)

        report_func.DumpReports(orig_report_func)
        report_func = orig_report_func
  else:
    raise Exception('Unknown file type: ' + apk_path)

  # Report dex stats outside of _AnalyzeInternal() so that the "unique methods"
  # metric is not just the sum of the base and chrome modules.
  for metric, count in dex_stats_collector.GetTotalCounts().items():
    report_func('Dex', metric, count, 'entries')
  report_func('Dex', 'unique methods',
              dex_stats_collector.GetUniqueMethodCount(), 'entries')
  report_func('DexCache', 'DexCache',
              dex_stats_collector.GetDexCacheSize(pre_oreo=sdk_version < 26),
              'bytes')

  return dex_stats_collector


def _ResourceSizes(args):
  chartjson = _BASE_CHART.copy() if args.output_format else None
  reporter = _ChartJsonReporter(chartjson)
  # Create DexStatsCollector here to track unique methods across trichrome APKs.
  dex_stats_collector = method_count.DexStatsCollector()

  specs = [
      ('Chrome_', args.trichrome_chrome),
      ('WebView_', args.trichrome_webview),
      ('Library_', args.trichrome_library),
  ]
  for prefix, path in specs:
    if path:
      reporter.trace_title_prefix = prefix
      child_dex_stats_collector = _AnalyzeApkOrApks(reporter, path,
                                                    args.out_dir)
      dex_stats_collector.MergeFrom(prefix, child_dex_stats_collector)

  if any(path for _, path in specs):
    reporter.SynthesizeTotals(dex_stats_collector.GetUniqueMethodCount())
  else:
    _AnalyzeApkOrApks(reporter, args.input, args.out_dir)

  if chartjson:
    _DumpChartJson(args, chartjson)


def _DumpChartJson(args, chartjson):
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
      raise Exception('chartjson conversion failed with error: ' +
                      histogram_result.stdout)

    histogram_path = os.path.join(args.output_dir, 'perf_results.json')
    logging.critical('Dumping histograms to %s', histogram_path)
    with open(histogram_path, 'wb') as json_file:
      json_file.write(histogram_result.stdout)


def main():
  build_utils.InitLogging('RESOURCE_SIZES_DEBUG')
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

  # Accepted to conform to the isolated script interface, but ignored.
  argparser.add_argument(
      '--isolated-script-test-filter', help=argparse.SUPPRESS)
  argparser.add_argument(
      '--isolated-script-test-perf-output',
      type=os.path.realpath,
      help=argparse.SUPPRESS)
  argparser.add_argument('--isolated-script-test-repeat',
                         help=argparse.SUPPRESS)
  argparser.add_argument('--isolated-script-test-launcher-retry-limit',
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
  trichrome_group = argparser.add_argument_group(
      'Trichrome inputs',
      description='When specified, |input| is used only as Test suite name.')
  trichrome_group.add_argument(
      '--trichrome-chrome', help='Path to Trichrome Chrome .apks')
  trichrome_group.add_argument(
      '--trichrome-webview', help='Path to Trichrome WebView .apk(s)')
  trichrome_group.add_argument(
      '--trichrome-library', help='Path to Trichrome Library .apk')
  args = argparser.parse_args()

  args.out_dir = _ConfigOutDir(args.out_dir)
  devil_chromium.Initialize(output_directory=args.out_dir)

  # TODO(bsheedy): Remove this once uses of --chartjson have been removed.
  if args.chartjson:
    args.output_format = 'chartjson'

  result_sink_client = result_sink.TryInitClient()
  isolated_script_output = {'valid': False, 'failures': []}

  test_name = 'resource_sizes (%s)' % os.path.basename(args.input)

  if args.isolated_script_test_output:
    args.output_dir = os.path.join(
        os.path.dirname(args.isolated_script_test_output), test_name)
    if not os.path.exists(args.output_dir):
      os.makedirs(args.output_dir)

  try:
    _ResourceSizes(args)
    isolated_script_output = {
        'valid': True,
        'failures': [],
    }
  finally:
    if args.isolated_script_test_output:
      results_path = os.path.join(args.output_dir, 'test_results.json')
      with open(results_path, 'w') as output_file:
        json.dump(isolated_script_output, output_file)
      with open(args.isolated_script_test_output, 'w') as output_file:
        json.dump(isolated_script_output, output_file)
    if result_sink_client:
      status = result_types.PASS
      if not isolated_script_output['valid']:
        status = result_types.UNKNOWN
      elif isolated_script_output['failures']:
        status = result_types.FAIL
      result_sink_client.Post(test_name, status, None, None, None)


if __name__ == '__main__':
  main()
