# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import argparse
import codecs
import datetime
import fnmatch
import glob
import json
import os
import plistlib
import re
import shutil
import subprocess
import stat
import sys
import tempfile

# Keys that should not be copied from mobileprovision
BANNED_KEYS = [
    "com.apple.developer.cs.allow-jit",
    "com.apple.developer.memory.transfer-send",
    "com.apple.developer.web-browser",
    "com.apple.developer.web-browser-engine.host",
    "com.apple.developer.web-browser-engine.networking",
    "com.apple.developer.web-browser-engine.rendering",
    "com.apple.developer.web-browser-engine.webcontent",
]

# Patterns for bundle extension per bundle type.
# Note: "ExtensionIcon83.5x83.5@2x.png" is one of the generated icon filename
# thus this pattern need to match decimal sized icons.
ICON_SUFFIX_PATTERN = '(\\d+(?:\\.\\d+)?)x\\1(@[23]x)?(~ipad)?\\.png'
BUNDLE_ICON_PATTERNS_MAP = {
    ".app": re.compile('AppIcon' + ICON_SUFFIX_PATTERN),
    ".appex": re.compile('ExtensionIcon' + ICON_SUFFIX_PATTERN),
}


if sys.version_info.major < 3:
  basestring_compat = basestring
else:
  basestring_compat = str


class FileListAction(argparse.Action):
  """Action that load a file and interpret it as a list of file names."""

  def __init__(self, option_strings, dest, nargs=None, **kwds):
    if nargs is not None:
      raise ValueError("nargs not allowed")
    super().__init__(option_strings, dest, nargs, **kwds)

  def __call__(self, parser, namespace, values, option_strings):
    dest = getattr(namespace, self.dest)
    with open(values, 'r', encoding='utf-8') as stream:
      for line in stream:
        path = line[:-1]
        dest.append(path)


def GetProvisioningProfilesDirs():
  """Returns the location of the installed mobile provisioning profiles.

  Returns:
    The paths to the directory containing the installed mobile provisioning
    profiles as a string.
  """
  paths = []
  paths.append(
      os.path.join(os.environ['HOME'], 'Library', 'MobileDevice',
                   'Provisioning Profiles'))
  # For Xcode 16 and later, include the new location,
  # `~/Library/Developer/Xcode/UserData/Provisioning Profiles`.
  paths.append(
      os.path.join(os.environ['HOME'], 'Library', 'Developer', 'Xcode',
                   'UserData', 'Provisioning Profiles'))
  return paths

def ReadPlistFromString(plist_bytes):
  """Parse property list from given |plist_bytes|.

    Args:
      plist_bytes: contents of property list to load. Must be bytes in python 3.

    Returns:
      The contents of property list as a python object.
    """
  if sys.version_info.major == 2:
    return plistlib.readPlistFromString(plist_bytes)
  else:
    return plistlib.loads(plist_bytes)


def LoadPlistFile(plist_path):
  """Loads property list file at |plist_path|.

  Args:
    plist_path: path to the property list file to load.

  Returns:
    The content of the property list file as a python object.
  """
  if sys.version_info.major == 2:
    return plistlib.readPlistFromString(
        subprocess.check_output(
            ['xcrun', 'plutil', '-convert', 'xml1', '-o', '-', plist_path]))
  else:
    with open(plist_path, 'rb') as fp:
      return plistlib.load(fp)


def CreateSymlink(value, location):
  """Creates symlink with value at location if the target exists."""
  target = os.path.join(os.path.dirname(location), value)
  if os.path.exists(location):
    os.unlink(location)
  os.symlink(value, location)


class Bundle(object):
  """Wraps a bundle."""

  def __init__(self, bundle_path, platform):
    """Initializes the Bundle object with data from bundle Info.plist file."""
    self._path = bundle_path
    self._kind = Bundle.Kind(platform, os.path.splitext(bundle_path)[-1])
    self._data = None

  def Load(self):
    self._data = LoadPlistFile(self.info_plist_path)

  @staticmethod
  def Kind(platform, extension):
    if platform in ('iphoneos', 'iphonesimulator'):
      return 'ios'
    if platform == 'macosx':
      if extension == '.framework':
        return 'mac_framework'
      return 'mac'
    if platform in ('watchos', 'watchsimulator'):
      return 'watchos'
    if platform in ('appletvos', 'appletvsimulator'):
      return 'tvos'
    raise ValueError('unknown bundle type %s for %s' % (extension, platform))

  @property
  def kind(self):
    return self._kind

  @property
  def path(self):
    return self._path

  @property
  def contents_dir(self):
    if self._kind == 'mac':
      return os.path.join(self.path, 'Contents')
    if self._kind == 'mac_framework':
      return os.path.join(self.path, 'Versions/A')
    return self.path

  @property
  def executable_dir(self):
    if self._kind == 'mac':
      return os.path.join(self.contents_dir, 'MacOS')
    return self.contents_dir

  @property
  def resources_dir(self):
    if self._kind == 'mac' or self._kind == 'mac_framework':
      return os.path.join(self.contents_dir, 'Resources')
    return self.path

  @property
  def info_plist_path(self):
    if self._kind == 'mac_framework':
      return os.path.join(self.resources_dir, 'Info.plist')
    return os.path.join(self.contents_dir, 'Info.plist')

  @property
  def signature_dir(self):
    return os.path.join(self.contents_dir, '_CodeSignature')

  @property
  def relative_signature_dir(self):
    return os.path.relpath(self.signature_dir, self.path)

  @property
  def embedded_mobileprovision(self):
    return os.path.join(self.path, 'embedded.mobileprovision')

  @property
  def relative_embedded_mobileprovision(self):
    return os.path.relpath(self.embedded_mobileprovision, self.path)

  @property
  def identifier(self):
    return self._data['CFBundleIdentifier']

  @property
  def binary_name(self):
    return self._data['CFBundleExecutable']

  @property
  def binary_path(self):
    return os.path.join(self.executable_dir, self.binary_name)

  def Validate(self, expected_mappings):
    """Checks that keys in the bundle have the expected value.

    Args:
      expected_mappings: a dictionary of string to object, each mapping will
      be looked up in the bundle data to check it has the same value (missing
      values will be ignored)

    Returns:
      A dictionary of the key with a different value between expected_mappings
      and the content of the bundle (i.e. errors) so that caller can format the
      error message. The dictionary will be empty if there are no errors.
    """
    errors = {}
    for key, expected_value in expected_mappings.items():
      if key in self._data:
        value = self._data[key]
        if value != expected_value:
          errors[key] = (value, expected_value)
    return errors


class ProvisioningProfile(object):
  """Wraps a mobile provisioning profile file."""

  def __init__(self, provisioning_profile_path):
    """Initializes the ProvisioningProfile with data from profile file."""
    self._path = provisioning_profile_path
    self._data = ReadPlistFromString(
        subprocess.check_output([
            'xcrun', 'security', 'cms', '-D', '-u', 'certUsageAnyCA', '-i',
            provisioning_profile_path
        ]))

  @property
  def path(self):
    return self._path

  @property
  def team_identifier(self):
    return self._data.get('TeamIdentifier', [''])[0]

  @property
  def name(self):
    return self._data.get('Name', '')

  @property
  def application_identifier_pattern(self):
    return self._data.get('Entitlements', {}).get('application-identifier', '')

  @property
  def application_identifier_prefix(self):
    return self._data.get('ApplicationIdentifierPrefix', [''])[0]

  @property
  def entitlements(self):
    return self._data.get('Entitlements', {})

  @property
  def expiration_date(self):
    return self._data.get('ExpirationDate', datetime.datetime.now())

  def ValidToSignBundle(self, bundle_identifier):
    """Checks whether the provisioning profile can sign bundle_identifier.

    Args:
      bundle_identifier: the identifier of the bundle that needs to be signed.

    Returns:
      True if the mobile provisioning profile can be used to sign a bundle
      with the corresponding bundle_identifier, False otherwise.
    """
    return fnmatch.fnmatch(
        '%s.%s' % (self.application_identifier_prefix, bundle_identifier),
        self.application_identifier_pattern)

  def Install(self, installation_path):
    """Copies mobile provisioning profile info to |installation_path|."""
    shutil.copy2(self.path, installation_path)
    st = os.stat(installation_path)
    os.chmod(installation_path, st.st_mode | stat.S_IWUSR)


class Entitlements(object):
  """Wraps an Entitlement plist file."""

  def __init__(self, entitlements_path):
    """Initializes Entitlements object from entitlement file."""
    self._path = entitlements_path
    self._data = LoadPlistFile(self._path)

  @property
  def path(self):
    return self._path

  def ExpandVariables(self, substitutions):
    self._data = self._ExpandVariables(self._data, substitutions)

  def _ExpandVariables(self, data, substitutions):
    if isinstance(data, basestring_compat):
      for key, substitution in substitutions.items():
        data = data.replace('$(%s)' % (key,), substitution)
      return data

    if isinstance(data, dict):
      for key, value in data.items():
        data[key] = self._ExpandVariables(value, substitutions)
      return data

    if isinstance(data, list):
      for i, value in enumerate(data):
        data[i] = self._ExpandVariables(value, substitutions)

    return data

  def LoadDefaults(self, defaults):
    for key, value in defaults.items():
      if key not in self._data and key not in BANNED_KEYS:
        self._data[key] = value

  def WriteTo(self, target_path):
    with open(target_path, 'wb') as fp:
      if sys.version_info.major == 2:
        plistlib.writePlist(self._data, fp)
      else:
        plistlib.dump(self._data, fp)


def FindProvisioningProfile(provisioning_profile_paths, bundle_identifier,
                            required):
  """Finds mobile provisioning profile to use to sign bundle.

  Args:
    bundle_identifier: the identifier of the bundle to sign.

  Returns:
    The ProvisioningProfile object that can be used to sign the Bundle
    object or None if no matching provisioning profile was found.
  """
  if not provisioning_profile_paths:
    for path in GetProvisioningProfilesDirs():
      provisioning_profile_paths.extend(
          glob.glob(os.path.join(path, '*.mobileprovision')))

  # Iterate over all installed mobile provisioning profiles and filter those
  # that can be used to sign the bundle, ignoring expired ones.
  now = datetime.datetime.now()
  valid_provisioning_profiles = []
  one_hour = datetime.timedelta(0, 3600)
  for provisioning_profile_path in provisioning_profile_paths:
    provisioning_profile = ProvisioningProfile(provisioning_profile_path)
    if provisioning_profile.expiration_date - now < one_hour:
      sys.stderr.write(
          'Warning: ignoring expired provisioning profile: %s.\n' %
          provisioning_profile_path)
      continue
    if provisioning_profile.ValidToSignBundle(bundle_identifier):
      valid_provisioning_profiles.append(provisioning_profile)

  if not valid_provisioning_profiles:
    if required:
      sys.stderr.write(
          'Error: no mobile provisioning profile found for "%s" in %s.\n' %
          (bundle_identifier, provisioning_profile_paths))
      sys.exit(1)
    return None

  # Select the most specific mobile provisioning profile, i.e. the one with
  # the longest application identifier pattern (prefer the one with the latest
  # expiration date as a secondary criteria).
  selected_provisioning_profile = max(
      valid_provisioning_profiles,
      key=lambda p: (len(p.application_identifier_pattern), p.expiration_date))

  one_week = datetime.timedelta(7)
  if selected_provisioning_profile.expiration_date - now < 2 * one_week:
    sys.stderr.write(
        'Warning: selected provisioning profile will expire soon: %s' %
        selected_provisioning_profile.path)
  return selected_provisioning_profile


def CodeSignBundle(bundle_path, identity, extra_args):
  process = subprocess.Popen(
      ['xcrun', 'codesign', '--force', '--sign', identity, '--timestamp=none'] +
      list(extra_args) + [bundle_path],
      stderr=subprocess.PIPE,
      universal_newlines=True)
  _, stderr = process.communicate()
  if process.returncode:
    sys.stderr.write(stderr)
    sys.exit(process.returncode)
  for line in stderr.splitlines():
    if line.endswith(': replacing existing signature'):
      # Ignore warning about replacing existing signature as this should only
      # happen when re-signing system frameworks (and then it is expected).
      continue
    sys.stderr.write(line)
    sys.stderr.write('\n')


def IsSubPath(path, parent_path):
  """Returns whether path is a sub-path of parent_path."""
  return path.startswith(parent_path + os.path.sep)


def DeleteItemAtPath(path):
  """Delete item at path.

  Support being called with a path pointing to a file, a symlink or a
  directory, calling the correct function for each situation.
  """
  if os.path.isdir(path):
    if not os.path.islink(path):
      shutil.rmtree(path)
      return
  os.unlink(path)


def VerifyBundleManifest(bundle, manifest):
  """Verify that bundle corresponds to manifest.

  If non-empty, then manifest is a list of all the files that should be present
  in the bundle (including the code signature that will be generated by this
  script).

  Does nothing if the manifest is empty. Otherwise, delete all files found in
  the bundle directory that are not listed in the manifest and terminate with
  an error if any files listed in the manifest is missing.
  """
  if not manifest:
    return

  # Ignore the files and directories created by the script when codesigning.
  # They will be listed in the manifest, but will be created after checking
  # the validity of the bundle.
  patterns = [
      lambda p: p == bundle.relative_embedded_mobileprovision,
      lambda p: IsSubPath(p, bundle.relative_signature_dir),
  ]
  filtered = lambda path: any(map(lambda pattern: pattern(path), patterns))
  manifest = set(path for path in manifest if not filtered(path))

  # Create a set of all directories in the manifest. Used to avoid doing
  # a linear scan of all files when a directory is found that may not be
  # present in the bundle (as that would cause the script to have O(n^2)
  # behaviour, since this check would happen for all directories).
  #
  # Note: since manifest only list files and maybe some directories that
  # should be considered as files (see comments below when processing
  # dirnames), it is required to iterate over all parent directories here.
  #
  # E.g if the manifest contains the following:
  #   'Foo'
  #   'data/test/files/file1'
  #   'data/test/files/file2'
  #   'data/test/files/file3'
  #
  # then manifest_directories should contain the following
  #   'data'
  #   'data/test'
  #   'data/test/files'
  manifest_directories = set()
  for path in manifest:
    dirname = os.path.dirname(path)
    while dirname and dirname not in manifest_directories:
      manifest_directories.add(dirname)
      dirname = os.path.dirname(dirname)

  # The bundle may contain a set of icons which will not be listed in the
  # manifest (this is because they are conditionally based on the sources
  # passed to the build/toolchain/apple/compile_xcassets.py script). Skip
  # them if present.
  bundle_icon_pattern = BUNDLE_ICON_PATTERNS_MAP.get(
      os.path.splitext(bundle.path)[-1], None)

  # Iterate over the content of the bundle.
  for dirpath, dirnames, filenames in os.walk(bundle.path):
    reldirpath = os.path.relpath(dirpath, bundle.path)

    # For directories, if they are listed explicitly in the manifest (and
    # not individual files), then skip them. This is necessary due to how
    # embedded frameworks work (there is a single bundle_data(...) target
    # that list the top-level bundle directory as the only source file).
    dirnames_to_skip = []
    for dirname in dirnames:
      subdirpath = os.path.normpath(os.path.join(reldirpath, dirname))
      if subdirpath in manifest:
        dirnames_to_skip.append(dirname)
        manifest.remove(subdirpath)
      elif subdirpath not in manifest_directories:
        dirnames_to_skip.append(dirname)
        print(f'warning: deleting old directory: {subdirpath}', file=sys.stderr)
        DeleteItemAtPath(os.path.join(dirpath, dirname))
    if dirnames_to_skip:
      dirnames[:] = list(set(dirnames) - set(dirnames_to_skip))

    # For files, if they are not listed by the manifest, delete them and
    # print a warning (this is not an error because they may be left-over
    # from an incremental build after changing the target dependencies).
    for filename in filenames:
      filepath = os.path.normpath(os.path.join(reldirpath, filename))
      if not filepath in manifest:
        if not bundle_icon_pattern or not bundle_icon_pattern.match(filename):
          print(f'warning: deleting old file: {filepath}', file=sys.stderr)
          DeleteItemAtPath(os.path.join(dirpath, filename))
      else:
        manifest.remove(filepath)

  # At this point, any files still listed in manifest is missing from the
  # bundle, so report this as an error and terminate the script with error.
  if manifest:
    print(f'error: {len(manifest)} missing files:', file=sys.stderr)
    for filepath in sorted(manifest):
      print(f'  - {filepath}', file=sys.stderr)
    sys.exit(1)


def InstallSystemFramework(framework_path, bundle_path, args):
  """Install framework from |framework_path| to |bundle| and code-re-sign it."""
  installed_framework_path = os.path.join(
      bundle_path, 'Frameworks', os.path.basename(framework_path))

  if os.path.isfile(framework_path):
    shutil.copy(framework_path, installed_framework_path)
  elif os.path.isdir(framework_path):
    if os.path.exists(installed_framework_path):
      shutil.rmtree(installed_framework_path)
    shutil.copytree(framework_path, installed_framework_path)

  CodeSignBundle(installed_framework_path, args.identity,
      ['--deep', '--preserve-metadata=identifier,entitlements,flags'])


def VerifyLoadOrder(binary_path, expected_first_framework):
  """Verifies that the first LC_LOAD_DYLIB in binary_path matches
  expected_first_framework.
  """
  try:
    output = subprocess.check_output(['otool', '-l', binary_path],
                                     stderr=subprocess.STDOUT,
                                     universal_newlines=True)
  except subprocess.CalledProcessError as e:
    sys.stderr.write('otool failed: %s\n' % e.output)
    sys.exit(1)

  first_dylib = None
  lines = output.splitlines()
  for i, line in enumerate(lines):
    if line.strip() == 'cmd LC_LOAD_DYLIB':
      # The name is usually a few lines down.
      for j in range(i + 1, min(i + 10, len(lines))):
        if lines[j].strip().startswith('name '):
          # Extract path. Format: name /path/to/lib (offset 24)
          parts = lines[j].strip().split(' ', 1)
          if len(parts) > 1:
            name_line = parts[1]
            # Remove " (offset \d+)"
            first_dylib = name_line.rsplit(' (offset', 1)[0]
          break
      if first_dylib:
        break

  if not first_dylib:
    sys.stderr.write(
        'Error: No LC_LOAD_DYLIB found in %s, but expected %s to be first.\n' %
        (binary_path, expected_first_framework))
    sys.exit(1)

  # The framework path ends with .../FrameworkName.framework/FrameworkName
  expected_suffix = '/%s.framework/%s' % (expected_first_framework,
                                          expected_first_framework)
  if not first_dylib.endswith(expected_suffix):
    sys.stderr.write(
        'Error: First LC_LOAD_DYLIB in %s is "%s", expected to end with "%s".\n'
        % (binary_path, first_dylib, expected_suffix))
    sys.exit(1)


def GenerateEntitlements(path, provisioning_profile, bundle_identifier):
  """Generates an entitlements file.

  Args:
    path: path to the entitlements template file
    provisioning_profile: ProvisioningProfile object to use, may be None
    bundle_identifier: identifier of the bundle to sign.
  """
  entitlements = Entitlements(path)
  if provisioning_profile:
    entitlements.LoadDefaults(provisioning_profile.entitlements)
    app_identifier_prefix = \
      provisioning_profile.application_identifier_prefix + '.'
  else:
    app_identifier_prefix = '*.'
  entitlements.ExpandVariables({
      'CFBundleIdentifier': bundle_identifier,
      'AppIdentifierPrefix': app_identifier_prefix,
  })
  return entitlements


def GenerateBundleInfoPlist(bundle, plist_compiler, partial_plist):
  """Generates the bundle Info.plist for a list of partial .plist files.

  Args:
    bundle: a Bundle instance
    plist_compiler: string, path to the Info.plist compiler
    partial_plist: list of path to partial .plist files to merge
  """

  # Filter empty partial .plist files (this happens if an application
  # does not compile any asset catalog, in which case the partial .plist
  # file from the asset catalog compilation step is just a stamp file).
  filtered_partial_plist = []
  for plist in partial_plist:
    plist_size = os.stat(plist).st_size
    if plist_size:
      filtered_partial_plist.append(plist)

  # Invoke the plist_compiler script. It needs to be a python script.
  subprocess.check_call([
      'python3',
      plist_compiler,
      'merge',
      '-f',
      'binary1',
      '-o',
      bundle.info_plist_path,
  ] + filtered_partial_plist)


class Action(object):
  """Class implementing one action supported by the script."""

  @classmethod
  def Register(cls, subparsers):
    parser = subparsers.add_parser(cls.name, help=cls.help)
    parser.set_defaults(func=cls._Execute)
    cls._Register(parser)


class CodeSignBundleAction(Action):
  """Class implementing the code-sign-bundle action."""

  name = 'code-sign-bundle'
  help = 'perform code signature for a bundle'

  @staticmethod
  def _Register(parser):
    parser.add_argument(
        '--entitlements', '-e', dest='entitlements_path',
        help='path to the entitlements file to use')
    parser.add_argument(
        'path', help='path to the iOS bundle to codesign')
    parser.add_argument(
        '--identity', '-i', required=True,
        help='identity to use to codesign')
    parser.add_argument(
        '--binary', '-b', required=True,
        help='path to the iOS bundle binary')
    parser.add_argument(
        '--framework', '-F', action='append', default=[], dest='frameworks',
        help='install and resign system framework')
    parser.add_argument(
        '--disable-code-signature', action='store_true', dest='no_signature',
        help='disable code signature')
    parser.add_argument(
        '--disable-embedded-mobileprovision', action='store_false',
        default=True, dest='embedded_mobileprovision',
        help='disable finding and embedding mobileprovision')
    parser.add_argument(
        '--platform', '-t', required=True,
        help='platform the signed bundle is targeting')
    parser.add_argument(
        '--partial-info-plist', '-p', action='append', default=[],
        help='path to partial Info.plist to merge to create bundle Info.plist')
    parser.add_argument(
        '--plist-compiler-path', '-P', action='store',
        help='path to the plist compiler script (for --partial-info-plist)')
    parser.add_argument(
        '--mobileprovision',
        '-m',
        action='append',
        default=[],
        dest='mobileprovision_files',
        help='list of mobileprovision files to use. If empty, uses the files ' +
        'in $HOME/Library/MobileDevice/Provisioning Profiles')
    parser.add_argument(
        '--mobileprovision-list',
        '-M',
        action=FileListAction,
        dest='mobileprovision_files',
        help='path to a file containing a list of mobileprovision files to ' +
        'use (this will behave as each "-m $line" was passsed for each line ' +
        'in that file)')
    parser.add_argument(
        '--manifest',
        '-L',
        default=[],
        action=FileListAction,
        dest='manifest',
        help='if present, path to a file containing the list of files that ' +
        'are part of the bundle to codesign. The script will delete any ' +
        'files found that are not listed, and will fail if any files is ' +
        'missing.')
    parser.add_argument(
        '--verify-load-order-first',
        dest='verify_load_order_first',
        help='verify that the named framework is the first loaded dylib')
    parser.set_defaults(no_signature=False)

  @staticmethod
  def _Execute(args):
    if not args.identity:
      args.identity = '-'

    bundle = Bundle(args.path, args.platform)

    if args.partial_info_plist:
      GenerateBundleInfoPlist(bundle, args.plist_compiler_path,
                              args.partial_info_plist)

    # The bundle Info.plist may have been updated by GenerateBundleInfoPlist()
    # above. Load the bundle information from Info.plist after the modification
    # have been written to disk.
    bundle.Load()

    # According to Apple documentation, the application binary must be the same
    # as the bundle name without the .app suffix. See crbug.com/740476 for more
    # information on what problem this can cause.
    #
    # To prevent this class of error, fail with an error if the binary name is
    # incorrect in the Info.plist as it is not possible to update the value in
    # Info.plist at this point (the file has been copied by a different target
    # and ninja would consider the build dirty if it was updated).
    #
    # Also checks that the name of the bundle is correct too (does not cause the
    # build to be considered dirty, but still terminate the script in case of an
    # incorrect bundle name).
    #
    # Apple documentation is available at:
    # https://developer.apple.com/library/content/documentation/CoreFoundation/Conceptual/CFBundles/BundleTypes/BundleTypes.html
    bundle_name = os.path.splitext(os.path.basename(bundle.path))[0]
    errors = bundle.Validate({
        'CFBundleName': bundle_name,
        'CFBundleExecutable': bundle_name,
    })
    if errors:
      for key in sorted(errors):
        value, expected_value = errors[key]
        sys.stderr.write('%s: error: %s value incorrect: %s != %s\n' % (
            bundle.path, key, value, expected_value))
      sys.stderr.flush()
      sys.exit(1)

    # Delete existing embedded mobile provisioning.
    embedded_provisioning_profile = bundle.embedded_mobileprovision
    if os.path.isfile(embedded_provisioning_profile):
      os.unlink(embedded_provisioning_profile)

    # Delete existing code signature.
    if os.path.exists(bundle.signature_dir):
      shutil.rmtree(bundle.signature_dir)

    # Install system frameworks if requested.
    for framework_path in args.frameworks:
      InstallSystemFramework(framework_path, args.path, args)

    # Copy main binary into bundle.
    if not os.path.isdir(bundle.executable_dir):
      os.makedirs(bundle.executable_dir)
    shutil.copy(args.binary, bundle.binary_path)

    # Record the symlinks created (they are likely not listed in the
    # manifest, but must not be deleted).
    created_symlinks = []

    if bundle.kind == 'mac_framework':
      # Create Versions/Current -> Versions/A symlink
      created_symlinks.append('Versions/Current')
      CreateSymlink('A', os.path.join(bundle.path, 'Versions/Current'))

      # Create $binary_name -> Versions/Current/$binary_name symlink
      created_symlinks.append(bundle.binary_name)
      CreateSymlink(os.path.join('Versions/Current', bundle.binary_name),
                    os.path.join(bundle.path, bundle.binary_name))

      # Create optional symlinks.
      for name in ('Headers', 'Resources', 'Modules'):
        target = os.path.join(bundle.path, 'Versions/A', name)
        if os.path.exists(target):
          created_symlinks.append(name)
          CreateSymlink(os.path.join('Versions/Current', name),
                        os.path.join(bundle.path, name))
        else:
          obsolete_path = os.path.join(bundle.path, name)
          if os.path.exists(obsolete_path):
            os.unlink(obsolete_path)

    # If the manifest is present, check that the bundle is well-formed. Only
    # perform this verification if requested, but in that case, consider all
    # the created symlinks as part of the manifest (since they are generated
    # conditionally, it is difficult to explicit list them all).
    if args.manifest:
      VerifyBundleManifest(bundle, set(args.manifest) | set(created_symlinks))

    if args.verify_load_order_first:
      VerifyLoadOrder(bundle.binary_path, args.verify_load_order_first)

    if args.no_signature:
      return

    codesign_extra_args = []

    if args.embedded_mobileprovision:
      # Find mobile provisioning profile and embeds it into the bundle (if a
      # code signing identify has been provided, fails if no valid mobile
      # provisioning is found).
      provisioning_profile_required = args.identity != '-'
      provisioning_profile = FindProvisioningProfile(
          args.mobileprovision_files, bundle.identifier,
          provisioning_profile_required)
      if provisioning_profile and not args.platform.endswith('simulator'):
        provisioning_profile.Install(embedded_provisioning_profile)

        if args.entitlements_path is not None:
          temporary_entitlements_file = \
              tempfile.NamedTemporaryFile(suffix='.xcent')
          codesign_extra_args.extend(
              ['--entitlements', temporary_entitlements_file.name])

          entitlements = GenerateEntitlements(
              args.entitlements_path, provisioning_profile, bundle.identifier)
          entitlements.WriteTo(temporary_entitlements_file.name)

    CodeSignBundle(bundle.path, args.identity, codesign_extra_args)


class CodeSignFileAction(Action):
  """Class implementing code signature for a single file."""

  name = 'code-sign-file'
  help = 'code-sign a single file'

  @staticmethod
  def _Register(parser):
    parser.add_argument(
        'path', help='path to the file to codesign')
    parser.add_argument(
        '--identity', '-i', required=True,
        help='identity to use to codesign')
    parser.add_argument(
        '--output', '-o',
        help='if specified copy the file to that location before signing it')
    parser.set_defaults(sign=True)

  @staticmethod
  def _Execute(args):
    if not args.identity:
      args.identity = '-'

    install_path = args.path
    if args.output:

      if os.path.isfile(args.output):
        os.unlink(args.output)
      elif os.path.isdir(args.output):
        shutil.rmtree(args.output)

      if os.path.isfile(args.path):
        shutil.copy(args.path, args.output)
      elif os.path.isdir(args.path):
        shutil.copytree(args.path, args.output)

      install_path = args.output

    CodeSignBundle(install_path, args.identity,
      ['--deep', '--preserve-metadata=identifier,entitlements'])


class GenerateEntitlementsAction(Action):
  """Class implementing the generate-entitlements action."""

  name = 'generate-entitlements'
  help = 'generate entitlements file'

  @staticmethod
  def _Register(parser):
    parser.add_argument(
        '--entitlements', '-e', dest='entitlements_path',
        help='path to the entitlements file to use')
    parser.add_argument(
        'path', help='path to the entitlements file to generate')
    parser.add_argument(
        '--info-plist', '-p', required=True,
        help='path to the bundle Info.plist')
    parser.add_argument(
        '--mobileprovision',
        '-m',
        action='append',
        default=[],
        dest='mobileprovision_files',
        help='set of mobileprovision files to use. If empty, uses the files ' +
        'in $HOME/Library/MobileDevice/Provisioning Profiles')
    parser.add_argument(
        '--mobileprovision-list',
        '-M',
        action=FileListAction,
        dest='mobileprovision_files',
        help='path to a file containing a list of mobileprovision files to ' +
        'use (this will behave as each "-m $line" was passsed for each line ' +
        'in that file)')

  @staticmethod
  def _Execute(args):
    info_plist = LoadPlistFile(args.info_plist)
    bundle_identifier = info_plist['CFBundleIdentifier']
    provisioning_profile = FindProvisioningProfile(args.mobileprovision_files,
                                                   bundle_identifier, False)
    entitlements = GenerateEntitlements(
        args.entitlements_path, provisioning_profile, bundle_identifier)
    entitlements.WriteTo(args.path)


class FindProvisioningProfileAction(Action):
  """Class implementing the find-codesign-identity action."""

  name = 'find-provisioning-profile'
  help = 'find provisioning profile for use by Xcode project generator'

  @staticmethod
  def _Register(parser):
    parser.add_argument('--bundle-id',
                        '-b',
                        required=True,
                        help='bundle identifier')
    parser.add_argument(
        '--mobileprovision',
        '-m',
        action='append',
        default=[],
        dest='mobileprovision_files',
        help='set of mobileprovision files to use. If empty, uses the files ' +
        'in $HOME/Library/MobileDevice/Provisioning Profiles')
    parser.add_argument(
        '--mobileprovision-list',
        '-M',
        action=FileListAction,
        dest='mobileprovision_files',
        help='path to a file containing a list of mobileprovision files to ' +
        'use (this will behave as each "-m $line" was passsed for each line ' +
        'in that file)')

  @staticmethod
  def _Execute(args):
    provisioning_profile_info = {}
    provisioning_profile = FindProvisioningProfile(args.mobileprovision_files,
                                                   args.bundle_id, False)
    for key in ('team_identifier', 'name', 'path'):
      if provisioning_profile:
        provisioning_profile_info[key] = getattr(provisioning_profile, key)
      else:
        provisioning_profile_info[key] = ''
    print(json.dumps(provisioning_profile_info))


def Main():
  # Cache this codec so that plistlib can find it. See
  # https://crbug.com/999461#c12 for more details.
  codecs.lookup('utf-8')

  parser = argparse.ArgumentParser('codesign iOS bundles')
  subparsers = parser.add_subparsers()

  actions = [
      CodeSignBundleAction,
      CodeSignFileAction,
      GenerateEntitlementsAction,
      FindProvisioningProfileAction,
  ]

  for action in actions:
    action.Register(subparsers)

  args = parser.parse_args()
  args.func(args)


if __name__ == '__main__':
  sys.exit(Main())
