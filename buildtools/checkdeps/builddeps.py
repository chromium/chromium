#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Traverses the source tree, parses all found DEPS files, and constructs
a dependency rule table to be used by subclasses.

See README.md for the format of the deps file.
"""



import copy
import os.path
import posixpath
import subprocess

from rules import Rule, Rules


# Variable name used in the DEPS file to add or subtract include files from
# the module-level deps.
INCLUDE_RULES_VAR_NAME = 'include_rules'

# Variable name used in the DEPS file to add or subtract include files
# from module-level deps specific to files whose basename (last
# component of path) matches a given regular expression.
SPECIFIC_INCLUDE_RULES_VAR_NAME = 'specific_include_rules'

# Optionally present in the DEPS file to list subdirectories which should not
# be checked. This allows us to skip third party code, for example.
SKIP_SUBDIRS_VAR_NAME = 'skip_child_includes'

# Optionally discard rules from parent directories, similar to "noparent" in
# OWNERS files. For example, if //ash/components has "noparent = True" then
# it will not inherit rules from //ash/DEPS, forcing each //ash/component/foo
# to declare all its dependencies.
NOPARENT_VAR_NAME = 'noparent'


class DepsBuilderError(Exception):
    """Base class for exceptions in this module."""
    pass


def NormalizePath(path):
  """Returns a path normalized to how we write DEPS rules and compare paths."""
  return os.path.normcase(path).replace(os.path.sep, posixpath.sep)


def _GitSourceDirectories(base_directory):
  """Returns set of normalized paths to subdirectories containing sources
  managed by git."""
  base_dir_norm = NormalizePath(base_directory)
  git_source_directories = set([base_dir_norm])

  git_cmd = 'git.bat' if os.name == 'nt' else 'git'
  git_ls_files_cmd = [git_cmd, 'ls-files']
  # FIXME: Use a context manager in Python 3.2+
  popen = subprocess.Popen(git_ls_files_cmd,
                           stdout=subprocess.PIPE,
                           cwd=base_directory)
  try:
    try:
      for line in popen.stdout.read().decode('utf-8').splitlines():
        dir_path = os.path.join(base_directory, os.path.dirname(line))
        dir_path_norm = NormalizePath(dir_path)
        # Add the directory as well as all the parent directories,
        # stopping once we reach an already-listed directory.
        while dir_path_norm not in git_source_directories:
          git_source_directories.add(dir_path_norm)
          dir_path_norm = posixpath.dirname(dir_path_norm)
    finally:
      popen.stdout.close()
  finally:
    popen.wait()

  return git_source_directories


class DepsBuilder(object):
  """Parses include_rules from DEPS files."""

  def __init__(self,
               base_directory=None,
               extra_repos=[],
               verbose=False,
               being_tested=False,
               ignore_temp_rules=False,
               ignore_specific_rules=False):
    """Creates a new DepsBuilder.

    Args:
      base_directory: local path to root of checkout, e.g. C:\chr\src.
      verbose: Set to True for debug output.
      being_tested: Set to True to ignore the DEPS file at
                    buildtools/checkdeps/DEPS.
      ignore_temp_rules: Ignore rules that start with Rule.TEMP_ALLOW ("!").
    """
    base_directory = (base_directory or
                      os.path.join(os.path.dirname(__file__),
                      os.path.pardir, os.path.pardir))
    self.base_directory = os.path.abspath(base_directory)  # Local absolute path
    self.extra_repos = extra_repos
    self.verbose = verbose
    self._under_test = being_tested
    self._ignore_temp_rules = ignore_temp_rules
    self._ignore_specific_rules = ignore_specific_rules
    self._git_source_directories = None

    if os.path.exists(os.path.join(base_directory, '.git')):
      self.is_git = True
    elif os.path.exists(os.path.join(base_directory, '.svn')):
      self.is_git = False
    else:
      raise DepsBuilderError("%s is not a repository root" % base_directory)

    # Map of normalized directory paths to rules to use for those
    # directories, or None for directories that should be skipped.
    # Normalized is: absolute, lowercase, / for separator.
    self.directory_rules = {}
    self._ApplyDirectoryRulesAndSkipSubdirs(Rules(), self.base_directory)

  def _ApplyRules(self, existing_rules, includes, specific_includes,
                  cur_dir_norm):
    """Applies the given include rules, returning the new rules.

    Args:
      existing_rules: A set of existing rules that will be combined.
      include: The list of rules from the "include_rules" section of DEPS.
      specific_includes: E.g. {'.*_unittest\.cc': ['+foo', '-blat']} rules
                         from the "specific_include_rules" section of DEPS.
      cur_dir_norm: The current directory, normalized path. We will create an
                    implicit rule that allows inclusion from this directory.

    Returns: A new set of rules combining the existing_rules with the other
             arguments.
    """
    rules = copy.deepcopy(existing_rules)

    # First apply the implicit "allow" rule for the current directory.
    base_dir_norm = NormalizePath(self.base_directory)
    if not cur_dir_norm.startswith(base_dir_norm):
      raise Exception(
          'Internal error: base directory is not at the beginning for\n'
          '  %s and base dir\n'
          '  %s' % (cur_dir_norm, base_dir_norm))
    relative_dir = posixpath.relpath(cur_dir_norm, base_dir_norm)

    # Make the help string a little more meaningful.
    source = relative_dir or 'top level'
    rules.AddRule('+' + relative_dir,
                  relative_dir,
                  'Default rule for ' + source)

    def ApplyOneRule(rule_str, dependee_regexp=None):
      """Deduces a sensible description for the rule being added, and
      adds the rule with its description to |rules|.

      If we are ignoring temporary rules, this function does nothing
      for rules beginning with the Rule.TEMP_ALLOW character.
      """
      if self._ignore_temp_rules and rule_str.startswith(Rule.TEMP_ALLOW):
        return

      rule_block_name = 'include_rules'
      if dependee_regexp:
        rule_block_name = 'specific_include_rules'
      if relative_dir:
        rule_description = relative_dir + "'s %s" % rule_block_name
      else:
        rule_description = 'the top level %s' % rule_block_name
      rules.AddRule(rule_str, relative_dir, rule_description, dependee_regexp)

    # Apply the additional explicit rules.
    for rule_str in includes:
      ApplyOneRule(rule_str)

    # Finally, apply the specific rules.
    if self._ignore_specific_rules:
      return rules

    for regexp, specific_rules in specific_includes.items():
      for rule_str in specific_rules:
        ApplyOneRule(rule_str, regexp)

    return rules

  def _ApplyDirectoryRules(self, existing_rules, dir_path_local_abs):
    """Combines rules from the existing rules and the new directory.

    Any directory can contain a DEPS file. Top-level DEPS files can contain
    module dependencies which are used by gclient. We use these, along with
    additional include rules and implicit rules for the given directory, to
    come up with a combined set of rules to apply for the directory.

    Args:
      existing_rules: The rules for the parent directory. We'll add-on to these.
      dir_path_local_abs: The directory path that the DEPS file may live in (if
                          it exists). This will also be used to generate the
                          implicit rules. This is a local path.

    Returns: A 2-tuple of:
      (1) the combined set of rules to apply to the sub-tree,
      (2) a list of all subdirectories that should NOT be checked, as specified
          in the DEPS file (if any).
          Subdirectories are single words, hence no OS dependence.
    """
    dir_path_norm = NormalizePath(dir_path_local_abs)

    # Check the DEPS file in this directory.
    if self.verbose:
      print('Applying rules from', dir_path_local_abs)
    def FromImpl(*_):
      pass  # NOP function so "From" doesn't fail.

    def FileImpl(_):
      pass  # NOP function so "File" doesn't fail.

    class _VarImpl:
      def __init__(self, local_scope):
        self._local_scope = local_scope

      def Lookup(self, var_name):
        """Implements the Var syntax."""
        try:
          return self._local_scope['vars'][var_name]
        except KeyError:
          raise Exception('Var is not defined: %s' % var_name)

    local_scope = {}
    global_scope = {
      'File': FileImpl,
      'From': FromImpl,
      'Var': _VarImpl(local_scope).Lookup,
      'Str': str,
    }
    deps_file_path = os.path.join(dir_path_local_abs, 'DEPS')

    # The second conditional here is to disregard the
    # buildtools/checkdeps/DEPS file while running tests.  This DEPS file
    # has a skip_child_includes for 'testdata' which is necessary for
    # running production tests, since there are intentional DEPS
    # violations under the testdata directory.  On the other hand when
    # running tests, we absolutely need to verify the contents of that
    # directory to trigger those intended violations and see that they
    # are handled correctly.
    if os.path.isfile(deps_file_path) and not (
        self._under_test and
        os.path.basename(dir_path_local_abs) == 'checkdeps'):
      try:
        with open(deps_file_path) as file:
          exec(file.read(), global_scope, local_scope)
      except Exception as e:
        print(' Error reading %s: %s' % (deps_file_path, str(e)))
        raise
    elif self.verbose:
      print('  No deps file found in', dir_path_local_abs)

    # Even if a DEPS file does not exist we still invoke ApplyRules
    # to apply the implicit "allow" rule for the current directory
    include_rules = local_scope.get(INCLUDE_RULES_VAR_NAME, [])
    specific_include_rules = local_scope.get(SPECIFIC_INCLUDE_RULES_VAR_NAME,
                                             {})
    skip_subdirs = local_scope.get(SKIP_SUBDIRS_VAR_NAME, [])
    noparent = local_scope.get(NOPARENT_VAR_NAME, False)
    if noparent:
      parent_rules = Rules()
    else:
      parent_rules = existing_rules

    return (self._ApplyRules(parent_rules, include_rules,
                             specific_include_rules, dir_path_norm),
            skip_subdirs)

  def _ApplyDirectoryRulesAndSkipSubdirs(self, parent_rules,
                                         dir_path_local_abs):
    """Given |parent_rules| and a subdirectory |dir_path_local_abs| of the
    directory that owns the |parent_rules|, add |dir_path_local_abs|'s rules to
    |self.directory_rules|, and add None entries for any of its
    subdirectories that should be skipped.
    """
    directory_rules, excluded_subdirs = self._ApplyDirectoryRules(
        parent_rules, dir_path_local_abs)
    dir_path_norm = NormalizePath(dir_path_local_abs)
    self.directory_rules[dir_path_norm] = directory_rules
    for subdir in excluded_subdirs:
      subdir_path_norm = posixpath.join(dir_path_norm, subdir)
      self.directory_rules[subdir_path_norm] = None

  def GetAllRulesAndFiles(self, dir_name=None):
    """Yields (rules, filenames) for each repository directory with DEPS rules.

    This walks the directory tree while staying in the repository. Specify
    |dir_name| to walk just one directory and its children; omit |dir_name| to
    walk the entire repository.

    Yields:
      Two-element (rules, filenames) tuples. |rules| is a rules.Rules object
      for a directory, and |filenames| is a list of the absolute local paths
      of all files in that directory.
    """
    if self.is_git and self._git_source_directories is None:
      self._git_source_directories = _GitSourceDirectories(self.base_directory)
      for repo in self.extra_repos:
        repo_path = os.path.join(self.base_directory, repo)
        self._git_source_directories.update(_GitSourceDirectories(repo_path))

    # Collect a list of all files and directories to check.
    if dir_name and not os.path.isabs(dir_name):
      dir_name = os.path.join(self.base_directory, dir_name)
    dirs_to_check = [dir_name or self.base_directory]
    while dirs_to_check:
      current_dir = dirs_to_check.pop()

      # Check that this directory is part of the source repository. This
      # prevents us from descending into third-party code or directories
      # generated by the build system.
      if self.is_git:
        if NormalizePath(current_dir) not in self._git_source_directories:
          continue
      elif not os.path.exists(os.path.join(current_dir, '.svn')):
        continue

      current_dir_rules = self.GetDirectoryRules(current_dir)

      if not current_dir_rules:
        continue  # Handle the 'skip_child_includes' case.

      current_dir_contents = sorted(os.listdir(current_dir))
      file_names = []
      sub_dirs = []
      for file_name in current_dir_contents:
        full_name = os.path.join(current_dir, file_name)
        if os.path.isdir(full_name):
          sub_dirs.append(full_name)
        else:
          file_names.append(full_name)
      dirs_to_check.extend(reversed(sub_dirs))

      yield (current_dir_rules, file_names)

  def GetDirectoryRules(self, dir_path_local):
    """Returns a Rules object to use for the given directory, or None
    if the given directory should be skipped.

    Also modifies |self.directory_rules| to store the Rules.
    This takes care of first building rules for parent directories (up to
    |self.base_directory|) if needed, which may add rules for skipped
    subdirectories.

    Args:
      dir_path_local: A local path to the directory you want rules for.
        Can be relative and unnormalized. It is the caller's responsibility
        to ensure that this is part of the repository rooted at
        |self.base_directory|.
    """
    if os.path.isabs(dir_path_local):
      dir_path_local_abs = dir_path_local
    else:
      dir_path_local_abs = os.path.join(self.base_directory, dir_path_local)
    dir_path_norm = NormalizePath(dir_path_local_abs)

    if dir_path_norm in self.directory_rules:
      return self.directory_rules[dir_path_norm]

    parent_dir_local_abs = os.path.dirname(dir_path_local_abs)
    parent_rules = self.GetDirectoryRules(parent_dir_local_abs)
    # We need to check for an entry for our dir_path again, since
    # GetDirectoryRules can modify entries for subdirectories, namely setting
    # to None if they should be skipped, via _ApplyDirectoryRulesAndSkipSubdirs.
    # For example, if dir_path == 'A/B/C' and A/B/DEPS specifies that the C
    # subdirectory be skipped, GetDirectoryRules('A/B') will fill in the entry
    # for 'A/B/C' as None.
    if dir_path_norm in self.directory_rules:
      return self.directory_rules[dir_path_norm]

    if parent_rules:
      self._ApplyDirectoryRulesAndSkipSubdirs(parent_rules, dir_path_local_abs)
    else:
      # If the parent directory should be skipped, then the current
      # directory should also be skipped.
      self.directory_rules[dir_path_norm] = None
    return self.directory_rules[dir_path_norm]
