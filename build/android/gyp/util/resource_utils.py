# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import contextlib
import itertools
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from xml.etree import ElementTree

import util.build_utils as build_utils

_SOURCE_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
# Import jinja2 from third_party/jinja2
sys.path.insert(1, os.path.join(_SOURCE_ROOT, 'third_party'))
from jinja2 import Template # pylint: disable=F0401


# A variation of these maps also exists in:
# //base/android/java/src/org/chromium/base/LocaleUtils.java
# //ui/android/java/src/org/chromium/base/LocalizationUtils.java
_CHROME_TO_ANDROID_LOCALE_MAP = {
    'es-419': 'es-rUS',
    'sr-Latn': 'b+sr+Latn',
    'fil': 'tl',
    'he': 'iw',
    'id': 'in',
    'yi': 'ji',
}
_ANDROID_TO_CHROMIUM_LANGUAGE_MAP = {
    'tl': 'fil',
    'iw': 'he',
    'in': 'id',
    'ji': 'yi',
    'no': 'nb',  # 'no' is not a real language. http://crbug.com/920960
}

_ALL_RESOURCE_TYPES = {
    'anim', 'animator', 'array', 'attr', 'bool', 'color', 'dimen', 'drawable',
    'font', 'fraction', 'id', 'integer', 'interpolator', 'layout', 'menu',
    'mipmap', 'plurals', 'raw', 'string', 'style', 'styleable', 'transition',
    'xml'
}

AAPT_IGNORE_PATTERN = ':'.join([
    '*OWNERS',  # Allow OWNERS files within res/
    'DIR_METADATA', # Allow DIR_METADATA files within res/
    '*.py',  # PRESUBMIT.py sometimes exist.
    '*.pyc',
    '*~',  # Some editors create these as temp files.
    '.*',  # Never makes sense to include dot(files/dirs).
    '*.d.stamp',  # Ignore stamp files
    '*.backup',  # Some tools create temporary backup files.
])

MULTIPLE_RES_MAGIC_STRING = b'magic'


def ToAndroidLocaleName(chromium_locale):
  """Convert a Chromium locale name into a corresponding Android one."""
  # Should be in sync with build/config/locales.gni.
  # First handle the special cases, these are needed to deal with Android
  # releases *before* 5.0/Lollipop.
  android_locale = _CHROME_TO_ANDROID_LOCALE_MAP.get(chromium_locale)
  if android_locale:
    return android_locale

  # Format of Chromium locale name is '<lang>' or '<lang>-<region>'
  # where <lang> is a 2 or 3 letter language code (ISO 639-1 or 639-2)
  # and region is a capitalized locale region name.
  lang, _, region = chromium_locale.partition('-')
  if not region:
    return lang

  # Translate newer language tags into obsolete ones. Only necessary if
  #  region is not None (e.g. 'he-IL' -> 'iw-rIL')
  lang = _CHROME_TO_ANDROID_LOCALE_MAP.get(lang, lang)

  # Using '<lang>-r<region>' is now acceptable as a locale name for all
  # versions of Android.
  return '%s-r%s' % (lang, region)


# ISO 639 language code + optional ("-r" + capitalized region code).
# Note that before Android 5.0/Lollipop, only 2-letter ISO 639-1 codes
# are supported.
_RE_ANDROID_LOCALE_QUALIFIER_1 = re.compile(r'^([a-z]{2,3})(\-r([A-Z]+))?$')

# Starting with Android 7.0/Nougat, BCP 47 codes are supported but must
# be prefixed with 'b+', and may include optional tags.
#  e.g. 'b+en+US', 'b+ja+Latn', 'b+ja+Latn+JP'
_RE_ANDROID_LOCALE_QUALIFIER_2 = re.compile(r'^b\+([a-z]{2,3})(\+.+)?$')


def ToChromiumLocaleName(android_locale):
  """Convert an Android locale name into a Chromium one."""
  lang = None
  region = None
  script = None
  m = _RE_ANDROID_LOCALE_QUALIFIER_1.match(android_locale)
  if m:
    lang = m.group(1)
    if m.group(2):
      region = m.group(3)
  elif _RE_ANDROID_LOCALE_QUALIFIER_2.match(android_locale):
    # Split an Android BCP-47 locale (e.g. b+sr+Latn+RS)
    tags = android_locale.split('+')

    # The Lang tag is always the first tag.
    lang = tags[1]

    # The optional region tag is 2ALPHA or 3DIGIT tag in pos 1 or 2.
    # The optional script tag is 4ALPHA and always in pos 1.
    optional_tags = iter(tags[2:])

    next_tag = next(optional_tags, None)
    if next_tag and len(next_tag) == 4:
      script = next_tag
      next_tag = next(optional_tags, None)
    if next_tag and len(next_tag) < 4:
      region = next_tag

  if not lang:
    return None

  # Special case for es-rUS -> es-419
  if lang == 'es' and region == 'US':
    return 'es-419'

  lang = _ANDROID_TO_CHROMIUM_LANGUAGE_MAP.get(lang, lang)

  if script:
    lang = '%s-%s' % (lang, script)

  if not region:
    return lang

  return '%s-%s' % (lang, region)


def IsAndroidLocaleQualifier(string):
  """Returns true if |string| is a valid Android resource locale qualifier."""
  return (_RE_ANDROID_LOCALE_QUALIFIER_1.match(string)
          or _RE_ANDROID_LOCALE_QUALIFIER_2.match(string))


def FindLocaleInStringResourceFilePath(file_path):
  """Return Android locale name of a string resource file path.

  Args:
    file_path: A file path.
  Returns:
    If |file_path| is of the format '.../values-<locale>/<name>.xml', return
    the value of <locale> (and Android locale qualifier). Otherwise return None.
  """
  if not file_path.endswith('.xml'):
    return None
  prefix = 'values-'
  dir_name = os.path.basename(os.path.dirname(file_path))
  if not dir_name.startswith(prefix):
    return None
  qualifier = dir_name[len(prefix):]
  return qualifier if IsAndroidLocaleQualifier(qualifier) else None


def ToAndroidLocaleList(locale_list):
  """Convert a list of Chromium locales into the corresponding Android list."""
  return sorted(ToAndroidLocaleName(locale) for locale in locale_list)

# Represents a line from a R.txt file.
_TextSymbolEntry = collections.namedtuple('RTextEntry',
    ('java_type', 'resource_type', 'name', 'value'))


def _GenerateGlobs(pattern):
  # This function processes the aapt ignore assets pattern into a list of globs
  # to be used to exclude files using build_utils.MatchesGlob. It removes the
  # '!', which is used by aapt to mean 'not chatty' so it does not output if the
  # file is ignored (we dont output anyways, so it is not required). This
  # function does not handle the <dir> and <file> prefixes used by aapt and are
  # assumed not to be included in the pattern string.
  return pattern.replace('!', '').split(':')


def DeduceResourceDirsFromFileList(resource_files):
  """Return a list of resource directories from a list of resource files."""
  # Directory list order is important, cannot use set or other data structures
  # that change order. This is because resource files of the same name in
  # multiple res/ directories ellide one another (the last one passed is used).
  # Thus the order must be maintained to prevent non-deterministic and possibly
  # flakey builds.
  resource_dirs = []
  for resource_path in resource_files:
    # Resources are always 1 directory deep under res/.
    res_dir = os.path.dirname(os.path.dirname(resource_path))
    if res_dir not in resource_dirs:
      resource_dirs.append(res_dir)

  # Check if any resource_dirs are children of other ones. This indicates that a
  # file was listed that is not exactly 1 directory deep under res/.
  # E.g.:
  # sources = ["java/res/values/foo.xml", "java/res/README.md"]
  # ^^ This will cause "java" to be detected as resource directory.
  for a, b in itertools.permutations(resource_dirs, 2):
    if not os.path.relpath(a, b).startswith('..'):
      bad_sources = (s for s in resource_files
                     if os.path.dirname(os.path.dirname(s)) == b)
      msg = """\
Resource(s) found that are not in a proper directory structure:
  {}
All resource files must follow a structure of "$ROOT/$SUBDIR/$FILE"."""
      raise Exception(msg.format('\n  '.join(bad_sources)))

  return resource_dirs


def IterResourceFilesInDirectories(directories,
                                   ignore_pattern=AAPT_IGNORE_PATTERN):
  globs = _GenerateGlobs(ignore_pattern)
  for d in directories:
    for root, _, files in os.walk(d):
      for f in files:
        archive_path = f
        parent_dir = os.path.relpath(root, d)
        if parent_dir != '.':
          archive_path = os.path.join(parent_dir, f)
        path = os.path.join(root, f)
        if build_utils.MatchesGlob(archive_path, globs):
          continue
        yield path, archive_path


class ResourceInfoFile(object):
  """Helper for building up .res.info files."""

  def __init__(self):
    # Dict of archive_path -> source_path for the current target.
    self._entries = {}
    # List of (old_archive_path, new_archive_path) tuples.
    self._renames = []
    # We don't currently support using both AddMapping and MergeInfoFile.
    self._add_mapping_was_called = False

  def AddMapping(self, archive_path, source_path):
    """Adds a single |archive_path| -> |source_path| entry."""
    self._add_mapping_was_called = True
    # "values/" files do not end up in the apk except through resources.arsc.
    if archive_path.startswith('values'):
      return
    source_path = os.path.normpath(source_path)
    new_value = self._entries.setdefault(archive_path, source_path)
    if new_value != source_path:
      raise Exception('Duplicate AddMapping for "{}". old={} new={}'.format(
          archive_path, new_value, source_path))

  def RegisterRename(self, old_archive_path, new_archive_path):
    """Records an archive_path rename.

    |old_archive_path| does not need to currently exist in the mappings. Renames
    are buffered and replayed only when Write() is called.
    """
    if not old_archive_path.startswith('values'):
      self._renames.append((old_archive_path, new_archive_path))

  def MergeInfoFile(self, info_file_path):
    """Merges the mappings from |info_file_path| into this object.

    Any existing entries are overridden.
    """
    assert not self._add_mapping_was_called
    # Allows clobbering, which is used when overriding resources.
    with open(info_file_path) as f:
      self._entries.update(l.rstrip().split('\t') for l in f)

  def _ApplyRenames(self):
    applied_renames = set()
    ret = self._entries
    for rename_tup in self._renames:
      # Duplicate entries happen for resource overrides.
      # Use a "seen" set to ensure we still error out if multiple renames
      # happen for the same old_archive_path with different new_archive_paths.
      if rename_tup in applied_renames:
        continue
      applied_renames.add(rename_tup)
      old_archive_path, new_archive_path = rename_tup
      ret[new_archive_path] = ret[old_archive_path]
      del ret[old_archive_path]

    self._entries = None
    self._renames = None
    return ret

  def Write(self, info_file_path):
    """Applies renames and writes out the file.

    No other methods may be called after this.
    """
    entries = self._ApplyRenames()
    lines = []
    for archive_path, source_path in entries.items():
      lines.append('{}\t{}\n'.format(archive_path, source_path))
    with open(info_file_path, 'w') as info_file:
      info_file.writelines(sorted(lines))


def _ParseTextSymbolsFile(path, fix_package_ids=False):
  """Given an R.txt file, returns a list of _TextSymbolEntry.

  Args:
    path: Input file path.
    fix_package_ids: if True, 0x00 and 0x02 package IDs read from the file
      will be fixed to 0x7f.
  Returns:
    A list of _TextSymbolEntry instances.
  Raises:
    Exception: An unexpected line was detected in the input.
  """
  ret = []
  with open(path) as f:
    for line in f:
      m = re.match(r'(int(?:\[\])?) (\w+) (\w+) (.+)$', line)
      if not m:
        raise Exception('Unexpected line in R.txt: %s' % line)
      java_type, resource_type, name, value = m.groups()
      if fix_package_ids:
        value = _FixPackageIds(value)
      ret.append(_TextSymbolEntry(java_type, resource_type, name, value))
  return ret


def _FixPackageIds(resource_value):
  # Resource IDs for resources belonging to regular APKs have their first byte
  # as 0x7f (package id). However with webview, since it is not a regular apk
  # but used as a shared library, aapt is passed the --shared-resources flag
  # which changes some of the package ids to 0x00.  This function normalises
  # these (0x00) package ids to 0x7f, which the generated code in R.java changes
  # to the correct package id at runtime.  resource_value is a string with
  # either, a single value '0x12345678', or an array of values like '{
  # 0xfedcba98, 0x01234567, 0x56789abc }'
  return resource_value.replace('0x00', '0x7f')


def _GetRTxtResourceNames(r_txt_path):
  """Parse an R.txt file and extract the set of resource names from it."""
  return {entry.name for entry in _ParseTextSymbolsFile(r_txt_path)}


def GetRTxtStringResourceNames(r_txt_path):
  """Parse an R.txt file and the list of its string resource names."""
  return sorted({
      entry.name
      for entry in _ParseTextSymbolsFile(r_txt_path)
      if entry.resource_type == 'string'
  })


def GenerateStringResourcesAllowList(module_r_txt_path, allowlist_r_txt_path):
  """Generate a allowlist of string resource IDs.

  Args:
    module_r_txt_path: Input base module R.txt path.
    allowlist_r_txt_path: Input allowlist R.txt path.
  Returns:
    A dictionary mapping numerical resource IDs to the corresponding
    string resource names. The ID values are taken from string resources in
    |module_r_txt_path| that are also listed by name in |allowlist_r_txt_path|.
  """
  allowlisted_names = {
      entry.name
      for entry in _ParseTextSymbolsFile(allowlist_r_txt_path)
      if entry.resource_type == 'string'
  }
  return {
      int(entry.value, 0): entry.name
      for entry in _ParseTextSymbolsFile(module_r_txt_path)
      if entry.resource_type == 'string' and entry.name in allowlisted_names
  }


class RJavaBuildOptions:
  """A class used to model the various ways to build an R.java file.

  This is used to control which resource ID variables will be final or
  non-final, and whether an onResourcesLoaded() method will be generated
  to adjust the non-final ones, when the corresponding library is loaded
  at runtime.

  Note that by default, all resources are final, and there is no
  method generated, which corresponds to calling ExportNoResources().
  """
  def __init__(self):
    self.has_constant_ids = True
    self.resources_allowlist = None
    self.has_on_resources_loaded = False
    self.export_const_styleable = False
    self.final_package_id = None
    self.fake_on_resources_loaded = False

  def ExportNoResources(self):
    """Make all resource IDs final, and don't generate a method."""
    self.has_constant_ids = True
    self.resources_allowlist = None
    self.has_on_resources_loaded = False
    self.export_const_styleable = False

  def ExportAllResources(self):
    """Make all resource IDs non-final in the R.java file."""
    self.has_constant_ids = False
    self.resources_allowlist = None

  def ExportSomeResources(self, r_txt_file_path):
    """Only select specific resource IDs to be non-final.

    Args:
      r_txt_file_path: The path to an R.txt file. All resources named
        int it will be non-final in the generated R.java file, all others
        will be final.
    """
    self.has_constant_ids = True
    self.resources_allowlist = _GetRTxtResourceNames(r_txt_file_path)

  def ExportAllStyleables(self):
    """Make all styleable constants non-final, even non-resources ones.

    Resources that are styleable but not of int[] type are not actually
    resource IDs but constants. By default they are always final. Call this
    method to make them non-final anyway in the final R.java file.
    """
    self.export_const_styleable = True

  def GenerateOnResourcesLoaded(self, fake=False):
    """Generate an onResourcesLoaded() method.

    This Java method will be called at runtime by the framework when
    the corresponding library (which includes the R.java source file)
    will be loaded at runtime. This corresponds to the --shared-resources
    or --app-as-shared-lib flags of 'aapt package'.

    if |fake|, then the method will be empty bodied to compile faster. This
    useful for dummy R.java files that will eventually be replaced by real
    ones.
    """
    self.has_on_resources_loaded = True
    self.fake_on_resources_loaded = fake

  def SetFinalPackageId(self, package_id):
    """Sets a package ID to be used for resources marked final."""
    self.final_package_id = package_id

  def _MaybeRewriteRTxtPackageIds(self, r_txt_path):
    """Rewrites package IDs in the R.txt file if necessary.

    If SetFinalPackageId() was called, some of the resource IDs may have had
    their package ID changed. This function rewrites the R.txt file to match
    those changes.
    """
    if self.final_package_id is None:
      return

    entries = _ParseTextSymbolsFile(r_txt_path)
    with open(r_txt_path, 'w') as f:
      for entry in entries:
        value = entry.value
        if self._IsResourceFinal(entry):
          value = re.sub(r'0x(?:00|7f)',
                         '0x{:02x}'.format(self.final_package_id), value)
        f.write('{} {} {} {}\n'.format(entry.java_type, entry.resource_type,
                                       entry.name, value))

  def _IsResourceFinal(self, entry):
    """Determines whether a resource should be final or not.

  Args:
    entry: A _TextSymbolEntry instance.
  Returns:
    True iff the corresponding entry should be final.
  """
    if entry.resource_type == 'styleable' and entry.java_type != 'int[]':
      # A styleable constant may be exported as non-final after all.
      return not self.export_const_styleable
    elif not self.has_constant_ids:
      # Every resource is non-final
      return False
    elif not self.resources_allowlist:
      # No allowlist means all IDs are non-final.
      return True
    else:
      # Otherwise, only those in the
      return entry.name not in self.resources_allowlist


def CreateRJavaFiles(srcjar_dir,
                     package,
                     main_r_txt_file,
                     extra_res_packages,
                     rjava_build_options,
                     srcjar_out,
                     custom_root_package_name=None,
                     grandparent_custom_package_name=None,
                     extra_main_r_text_files=None,
                     ignore_mismatched_values=False):
  """Create all R.java files for a set of packages and R.txt files.

  Args:
    srcjar_dir: The top-level output directory for the generated files.
    package: Package name for R java source files which will inherit
      from the root R java file.
    main_r_txt_file: The main R.txt file containing the valid values
      of _all_ resource IDs.
    extra_res_packages: A list of extra package names.
    rjava_build_options: An RJavaBuildOptions instance that controls how
      exactly the R.java file is generated.
    srcjar_out: Path of desired output srcjar.
    custom_root_package_name: Custom package name for module root R.java file,
      (eg. vr for gen.vr package).
    grandparent_custom_package_name: Custom root package name for the root
      R.java file to inherit from. DFM root R.java files will have "base"
      as the grandparent_custom_package_name. The format of this package name
      is identical to custom_root_package_name.
      (eg. for vr grandparent_custom_package_name would be "base")
    extra_main_r_text_files: R.txt files to be added to the root R.java file.
    ignore_mismatched_values: If True, ignores if a resource appears multiple
      times with different entry values (useful when all the values are
      dummy anyways).
  Raises:
    Exception if a package name appears several times in |extra_res_packages|
  """
  rjava_build_options._MaybeRewriteRTxtPackageIds(main_r_txt_file)

  packages = list(extra_res_packages)

  if package and package not in packages:
    # Sometimes, an apk target and a resources target share the same
    # AndroidManifest.xml and thus |package| will already be in |packages|.
    packages.append(package)

  # Map of (resource_type, name) -> Entry.
  # Contains the correct values for resources.
  all_resources = {}
  all_resources_by_type = collections.defaultdict(list)

  main_r_text_files = [main_r_txt_file]
  if extra_main_r_text_files:
    main_r_text_files.extend(extra_main_r_text_files)
  for r_txt_file in main_r_text_files:
    for entry in _ParseTextSymbolsFile(r_txt_file, fix_package_ids=True):
      entry_key = (entry.resource_type, entry.name)
      if entry_key in all_resources:
        if not ignore_mismatched_values:
          assert entry == all_resources[entry_key], (
              'Input R.txt %s provided a duplicate resource with a different '
              'entry value. Got %s, expected %s.' %
              (r_txt_file, entry, all_resources[entry_key]))
      else:
        all_resources[entry_key] = entry
        all_resources_by_type[entry.resource_type].append(entry)
        assert entry.resource_type in _ALL_RESOURCE_TYPES, (
            'Unknown resource type: %s, add to _ALL_RESOURCE_TYPES!' %
            entry.resource_type)

  if custom_root_package_name:
    # Custom package name is available, thus use it for root_r_java_package.
    root_r_java_package = GetCustomPackagePath(custom_root_package_name)
  else:
    # Create a unique name using srcjar_out. Underscores are added to ensure
    # no reserved keywords are used for directory names.
    root_r_java_package = re.sub('[^\w\.]', '', srcjar_out.replace('/', '._'))

  root_r_java_dir = os.path.join(srcjar_dir, *root_r_java_package.split('.'))
  build_utils.MakeDirectory(root_r_java_dir)
  root_r_java_path = os.path.join(root_r_java_dir, 'R.java')
  root_java_file_contents = _RenderRootRJavaSource(
      root_r_java_package, all_resources_by_type, rjava_build_options,
      grandparent_custom_package_name)
  with open(root_r_java_path, 'w') as f:
    f.write(root_java_file_contents)

  for package in packages:
    _CreateRJavaSourceFile(srcjar_dir, package, root_r_java_package,
                           rjava_build_options)


def _CreateRJavaSourceFile(srcjar_dir, package, root_r_java_package,
                           rjava_build_options):
  """Generates an R.java source file."""
  package_r_java_dir = os.path.join(srcjar_dir, *package.split('.'))
  build_utils.MakeDirectory(package_r_java_dir)
  package_r_java_path = os.path.join(package_r_java_dir, 'R.java')
  java_file_contents = _RenderRJavaSource(package, root_r_java_package,
                                          rjava_build_options)
  with open(package_r_java_path, 'w') as f:
    f.write(java_file_contents)


# Resource IDs inside resource arrays are sorted. Application resource IDs start
# with 0x7f but system resource IDs start with 0x01 thus system resource ids are
# always at the start of the array. This function finds the index of the first
# non system resource id to be used for package ID rewriting (we should not
# rewrite system resource ids).
def _GetNonSystemIndex(entry):
  """Get the index of the first application resource ID within a resource
  array."""
  res_ids = re.findall(r'0x[0-9a-f]{8}', entry.value)
  for i, res_id in enumerate(res_ids):
    if res_id.startswith('0x7f'):
      return i
  return len(res_ids)


def _RenderRJavaSource(package, root_r_java_package, rjava_build_options):
  """Generates the contents of a R.java file."""
  template = Template(
      """/* AUTO-GENERATED FILE.  DO NOT MODIFY. */

package {{ package }};

public final class R {
    {% for resource_type in resource_types %}
    public static final class {{ resource_type }} extends
            {{ root_package }}.R.{{ resource_type }} {}
    {% endfor %}
    {% if has_on_resources_loaded %}
    public static void onResourcesLoaded(int packageId) {
        {{ root_package }}.R.onResourcesLoaded(packageId);
    }
    {% endif %}
}
""",
      trim_blocks=True,
      lstrip_blocks=True)

  return template.render(
      package=package,
      resource_types=sorted(_ALL_RESOURCE_TYPES),
      root_package=root_r_java_package,
      has_on_resources_loaded=rjava_build_options.has_on_resources_loaded)


def GetCustomPackagePath(package_name):
  return 'gen.' + package_name + '_module'


def _RenderRootRJavaSource(package, all_resources_by_type, rjava_build_options,
                           grandparent_custom_package_name):
  """Render an R.java source file. See _CreateRJaveSourceFile for args info."""
  final_resources_by_type = collections.defaultdict(list)
  non_final_resources_by_type = collections.defaultdict(list)
  for res_type, resources in all_resources_by_type.items():
    for entry in resources:
      # Entries in stylable that are not int[] are not actually resource ids
      # but constants.
      if rjava_build_options._IsResourceFinal(entry):
        final_resources_by_type[res_type].append(entry)
      else:
        non_final_resources_by_type[res_type].append(entry)

  # Keep these assignments all on one line to make diffing against regular
  # aapt-generated files easier.
  create_id = ('{{ e.resource_type }}.{{ e.name }} ^= packageIdTransform;')
  create_id_arr = ('{{ e.resource_type }}.{{ e.name }}[i] ^='
                   ' packageIdTransform;')
  for_loop_condition = ('int i = {{ startIndex(e) }}; i < '
                        '{{ e.resource_type }}.{{ e.name }}.length; ++i')

  # Here we diverge from what aapt does. Because we have so many
  # resources, the onResourcesLoaded method was exceeding the 64KB limit that
  # Java imposes. For this reason we split onResourcesLoaded into different
  # methods for each resource type.
  extends_string = ''
  dep_path = ''
  if grandparent_custom_package_name:
    extends_string = 'extends {{ parent_path }}.R.{{ resource_type }} '
    dep_path = GetCustomPackagePath(grandparent_custom_package_name)

  template = Template("""/* AUTO-GENERATED FILE.  DO NOT MODIFY. */

package {{ package }};

public final class R {
    {% for resource_type in resource_types %}
    public static class {{ resource_type }} """ + extends_string + """ {
        {% for e in final_resources[resource_type] %}
        public static final {{ e.java_type }} {{ e.name }} = {{ e.value }};
        {% endfor %}
        {% for e in non_final_resources[resource_type] %}
            {% if e.value != '0' %}
        public static {{ e.java_type }} {{ e.name }} = {{ e.value }};
            {% else %}
        public static {{ e.java_type }} {{ e.name }};
            {% endif %}
        {% endfor %}
    }
    {% endfor %}
    {% if has_on_resources_loaded %}
      {% if fake_on_resources_loaded %}
    public static void onResourcesLoaded(int packageId) {
    }
      {% else %}
    private static boolean sResourcesDidLoad;
    public static void onResourcesLoaded(int packageId) {
        if (sResourcesDidLoad) {
            return;
        }
        sResourcesDidLoad = true;
        int packageIdTransform = (packageId ^ 0x7f) << 24;
        {% for resource_type in resource_types %}
        onResourcesLoaded{{ resource_type|title }}(packageIdTransform);
        {% for e in non_final_resources[resource_type] %}
        {% if e.java_type == 'int[]' %}
        for(""" + for_loop_condition + """) {
            """ + create_id_arr + """
        }
        {% endif %}
        {% endfor %}
        {% endfor %}
    }
    {% for res_type in resource_types %}
    private static void onResourcesLoaded{{ res_type|title }} (
            int packageIdTransform) {
        {% for e in non_final_resources[res_type] %}
        {% if res_type != 'styleable' and e.java_type != 'int[]' %}
        """ + create_id + """
        {% endif %}
        {% endfor %}
    }
    {% endfor %}
      {% endif %}
    {% endif %}
}
""",
                      trim_blocks=True,
                      lstrip_blocks=True)
  return template.render(
      package=package,
      resource_types=sorted(_ALL_RESOURCE_TYPES),
      has_on_resources_loaded=rjava_build_options.has_on_resources_loaded,
      fake_on_resources_loaded=rjava_build_options.fake_on_resources_loaded,
      final_resources=final_resources_by_type,
      non_final_resources=non_final_resources_by_type,
      startIndex=_GetNonSystemIndex,
      parent_path=dep_path)


def ExtractBinaryManifestValues(aapt2_path, apk_path):
  """Returns (version_code, version_name, package_name) for the given apk."""
  output = subprocess.check_output([
      aapt2_path, 'dump', 'xmltree', apk_path, '--file', 'AndroidManifest.xml'
  ]).decode('utf-8')
  version_code = re.search(r'versionCode.*?=(\d*)', output).group(1)
  version_name = re.search(r'versionName.*?="(.*?)"', output).group(1)
  package_name = re.search(r'package.*?="(.*?)"', output).group(1)
  return version_code, version_name, package_name


def ExtractArscPackage(aapt2_path, apk_path):
  """Returns (package_name, package_id) of resources.arsc from apk_path."""
  proc = subprocess.Popen([aapt2_path, 'dump', 'resources', apk_path],
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE)
  for line in proc.stdout:
    line = line.decode('utf-8')
    # Package name=org.chromium.webview_shell id=7f
    if line.startswith('Package'):
      proc.kill()
      parts = line.split()
      package_name = parts[1].split('=')[1]
      package_id = parts[2][3:]
      return package_name, int(package_id, 16)

  # aapt2 currently crashes when dumping webview resources, but not until after
  # it prints the "Package" line (b/130553900).
  sys.stderr.write(proc.stderr.read())
  raise Exception('Failed to find arsc package name')


def _RenameSubdirsWithPrefix(dir_path, prefix):
  subdirs = [
      d for d in os.listdir(dir_path)
      if os.path.isdir(os.path.join(dir_path, d))
  ]
  renamed_subdirs = []
  for d in subdirs:
    old_path = os.path.join(dir_path, d)
    new_path = os.path.join(dir_path, '{}_{}'.format(prefix, d))
    renamed_subdirs.append(new_path)
    os.rename(old_path, new_path)
  return renamed_subdirs


def _HasMultipleResDirs(zip_path):
  """Checks for magic comment set by prepare_resources.py

  Returns: True iff the zipfile has the magic comment that means it contains
  multiple res/ dirs inside instead of just contents of a single res/ dir
  (without a wrapping res/).
  """
  with zipfile.ZipFile(zip_path) as z:
    return z.comment == MULTIPLE_RES_MAGIC_STRING


def ExtractDeps(dep_zips, deps_dir):
  """Extract a list of resource dependency zip files.

  Args:
     dep_zips: A list of zip file paths, each one will be extracted to
       a subdirectory of |deps_dir|, named after the zip file's path (e.g.
       '/some/path/foo.zip' -> '{deps_dir}/some_path_foo/').
    deps_dir: Top-level extraction directory.
  Returns:
    The list of all sub-directory paths, relative to |deps_dir|.
  Raises:
    Exception: If a sub-directory already exists with the same name before
      extraction.
  """
  dep_subdirs = []
  for z in dep_zips:
    subdirname = z.replace(os.path.sep, '_')
    subdir = os.path.join(deps_dir, subdirname)
    if os.path.exists(subdir):
      raise Exception('Resource zip name conflict: ' + subdirname)
    build_utils.ExtractAll(z, path=subdir)
    if _HasMultipleResDirs(z):
      # basename of the directory is used to create a zip during resource
      # compilation, include the path in the basename to help blame errors on
      # the correct target. For example directory 0_res may be renamed
      # chrome_android_chrome_app_java_resources_0_res pointing to the name and
      # path of the android_resources target from whence it came.
      subdir_subdirs = _RenameSubdirsWithPrefix(subdir, subdirname)
      dep_subdirs.extend(subdir_subdirs)
    else:
      dep_subdirs.append(subdir)
  return dep_subdirs


class _ResourceBuildContext(object):
  """A temporary directory for packaging and compiling Android resources.

  Args:
    temp_dir: Optional root build directory path. If None, a temporary
      directory will be created, and removed in Close().
  """

  def __init__(self, temp_dir=None, keep_files=False):
    """Initialized the context."""
    # The top-level temporary directory.
    if temp_dir:
      self.temp_dir = temp_dir
      os.makedirs(temp_dir)
    else:
      self.temp_dir = tempfile.mkdtemp()
    self.remove_on_exit = not keep_files

    # A location to store resources extracted form dependency zip files.
    self.deps_dir = os.path.join(self.temp_dir, 'deps')
    os.mkdir(self.deps_dir)
    # A location to place aapt-generated files.
    self.gen_dir = os.path.join(self.temp_dir, 'gen')
    os.mkdir(self.gen_dir)
    # A location to place generated R.java files.
    self.srcjar_dir = os.path.join(self.temp_dir, 'java')
    os.mkdir(self.srcjar_dir)
    # Temporary file locacations.
    self.r_txt_path = os.path.join(self.gen_dir, 'R.txt')
    self.srcjar_path = os.path.join(self.temp_dir, 'R.srcjar')
    self.info_path = os.path.join(self.temp_dir, 'size.info')
    self.stable_ids_path = os.path.join(self.temp_dir, 'in_ids.txt')
    self.emit_ids_path = os.path.join(self.temp_dir, 'out_ids.txt')
    self.proguard_path = os.path.join(self.temp_dir, 'keeps.flags')
    self.proguard_main_dex_path = os.path.join(self.temp_dir, 'maindex.flags')
    self.arsc_path = os.path.join(self.temp_dir, 'out.ap_')
    self.proto_path = os.path.join(self.temp_dir, 'out.proto.ap_')
    self.optimized_arsc_path = os.path.join(self.temp_dir, 'out.opt.ap_')
    self.optimized_proto_path = os.path.join(self.temp_dir, 'out.opt.proto.ap_')

  def Close(self):
    """Close the context and destroy all temporary files."""
    if self.remove_on_exit:
      shutil.rmtree(self.temp_dir)


@contextlib.contextmanager
def BuildContext(temp_dir=None, keep_files=False):
  """Generator for a _ResourceBuildContext instance."""
  context = None
  try:
    context = _ResourceBuildContext(temp_dir, keep_files)
    yield context
  finally:
    if context:
      context.Close()


def ResourceArgsParser():
  """Create an argparse.ArgumentParser instance with common argument groups.

  Returns:
    A tuple of (parser, in_group, out_group) corresponding to the parser
    instance, and the input and output argument groups for it, respectively.
  """
  parser = argparse.ArgumentParser(description=__doc__)

  input_opts = parser.add_argument_group('Input options')
  output_opts = parser.add_argument_group('Output options')

  build_utils.AddDepfileOption(output_opts)

  input_opts.add_argument('--include-resources', required=True, action="append",
                        help='Paths to arsc resource files used to link '
                             'against. Can be specified multiple times.')

  input_opts.add_argument('--dependencies-res-zips', required=True,
                    help='Resources zip archives from dependents. Required to '
                         'resolve @type/foo references into dependent '
                         'libraries.')

  input_opts.add_argument(
      '--r-text-in',
       help='Path to pre-existing R.txt. Its resource IDs override those found '
            'in the aapt-generated R.txt when generating R.java.')

  input_opts.add_argument(
      '--extra-res-packages',
      help='Additional package names to generate R.java files for.')

  return (parser, input_opts, output_opts)


def HandleCommonOptions(options):
  """Handle common command-line options after parsing.

  Args:
    options: the result of parse_args() on the parser returned by
        ResourceArgsParser(). This function updates a few common fields.
  """
  options.include_resources = [build_utils.ParseGnList(r) for r in
                               options.include_resources]
  # Flatten list of include resources list to make it easier to use.
  options.include_resources = [r for resources in options.include_resources
                               for r in resources]

  options.dependencies_res_zips = (
      build_utils.ParseGnList(options.dependencies_res_zips))

  # Don't use [] as default value since some script explicitly pass "".
  if options.extra_res_packages:
    options.extra_res_packages = (
        build_utils.ParseGnList(options.extra_res_packages))
  else:
    options.extra_res_packages = []


def ParseAndroidResourceStringsFromXml(xml_data):
  """Parse and Android xml resource file and extract strings from it.

  Args:
    xml_data: XML file data.
  Returns:
    A (dict, namespaces) tuple, where |dict| maps string names to their UTF-8
    encoded value, and |namespaces| is a dictionary mapping prefixes to URLs
    corresponding to namespaces declared in the <resources> element.
  """
  # NOTE: This uses regular expression matching because parsing with something
  # like ElementTree makes it tedious to properly parse some of the structured
  # text found in string resources, e.g.:
  #      <string msgid="3300176832234831527" \
  #         name="abc_shareactionprovider_share_with_application">\
  #             "Condividi tramite <ns1:g id="APPLICATION_NAME">%s</ns1:g>"\
  #      </string>
  result = {}

  # Find <resources> start tag and extract namespaces from it.
  m = re.search('<resources([^>]*)>', xml_data, re.MULTILINE)
  if not m:
    raise Exception('<resources> start tag expected: ' + xml_data)
  input_data = xml_data[m.end():]
  resource_attrs = m.group(1)
  re_namespace = re.compile('\s*(xmlns:(\w+)="([^"]+)")')
  namespaces = {}
  while resource_attrs:
    m = re_namespace.match(resource_attrs)
    if not m:
      break
    namespaces[m.group(2)] = m.group(3)
    resource_attrs = resource_attrs[m.end(1):]

  # Find each string element now.
  re_string_element_start = re.compile('<string ([^>]* )?name="([^">]+)"[^>]*>')
  re_string_element_end = re.compile('</string>')
  while input_data:
    m = re_string_element_start.search(input_data)
    if not m:
      break
    name = m.group(2)
    input_data = input_data[m.end():]
    m2 = re_string_element_end.search(input_data)
    if not m2:
      raise Exception('Expected closing string tag: ' + input_data)
    text = input_data[:m2.start()]
    input_data = input_data[m2.end():]
    if len(text) and text[0] == '"' and text[-1] == '"':
      text = text[1:-1]
    result[name] = text

  return result, namespaces


def GenerateAndroidResourceStringsXml(names_to_utf8_text, namespaces=None):
  """Generate an XML text corresponding to an Android resource strings map.

  Args:
    names_to_text: A dictionary mapping resource names to localized
      text (encoded as UTF-8).
    namespaces: A map of namespace prefix to URL.
  Returns:
    New non-Unicode string containing an XML data structure describing the
    input as an Android resource .xml file.
  """
  result = '<?xml version="1.0" encoding="utf-8"?>\n'
  result += '<resources'
  if namespaces:
    for prefix, url in sorted(namespaces.items()):
      result += ' xmlns:%s="%s"' % (prefix, url)
  result += '>\n'
  if not names_to_utf8_text:
    result += '<!-- this file intentionally empty -->\n'
  else:
    for name, utf8_text in sorted(names_to_utf8_text.items()):
      result += '<string name="%s">"%s"</string>\n' % (name, utf8_text)
  result += '</resources>\n'
  return result.encode('utf8')


def FilterAndroidResourceStringsXml(xml_file_path, string_predicate):
  """Remove unwanted localized strings from an Android resource .xml file.

  This function takes a |string_predicate| callable object that will
  receive a resource string name, and should return True iff the
  corresponding <string> element should be kept in the file.

  Args:
    xml_file_path: Android resource strings xml file path.
    string_predicate: A predicate function which will receive the string name
      and shal
  """
  with open(xml_file_path) as f:
    xml_data = f.read()
  strings_map, namespaces = ParseAndroidResourceStringsFromXml(xml_data)

  string_deletion = False
  for name in list(strings_map.keys()):
    if not string_predicate(name):
      del strings_map[name]
      string_deletion = True

  if string_deletion:
    new_xml_data = GenerateAndroidResourceStringsXml(strings_map, namespaces)
    with open(xml_file_path, 'wb') as f:
      f.write(new_xml_data)
