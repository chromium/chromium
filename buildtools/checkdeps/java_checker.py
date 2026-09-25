# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks Java files for illegal imports."""


import concurrent.futures
import os
import posixpath
import re
import subprocess

import results
from rules import Rule


class JavaChecker(object):
  """Import checker for Java files.

  The CheckFile method uses real filesystem paths, but Java imports work in
  terms of package names. To deal with this, we have an extra "prescan" pass
  that reads all the .java files and builds a mapping of class name -> filepath.
  In CheckFile, we convert each import statement into a real filepath, and check
  that against the rules in the DEPS files.

  Note that in Java you can always use classes in the same directory without an
  explicit import statement, so these imports can't be blocked with DEPS files.
  But that shouldn't be a problem, because same-package imports are pretty much
  always correct by definition. (If we find a case where this is *not* correct,
  it probably means the package is too big and needs to be split up.)

  Properties:
    _classmap: dict of fully-qualified Java class name -> filepath
  """

  EXTENSIONS = ['.java']

  # This regular expression will be used to extract filenames from import
  # statements.
  _EXTRACT_IMPORT_PATH = re.compile(r'^import\s+(?:static\s+)?([\w\.]+)\s*;')

  def __init__(self, base_directory, verbose, added_imports=None,
               allow_multiple_definitions=None):
    self._base_directory = base_directory
    self._verbose = verbose
    self._classmap = {}
    self._allow_multiple_definitions = allow_multiple_definitions or []
    if added_imports:
      added_classset = self._PrescanImportFiles(added_imports)
      self._PrescanFiles(added_classset, added_imports)

  def _GetClassFullName(self, filepath):
    """Get the full class name of a file with package name."""
    if not os.path.isfile(filepath):
      return None
    with open(filepath, encoding='utf-8') as f:
      short_class_name, _ = os.path.splitext(os.path.basename(filepath))
      for line in f:
        for package in re.findall(r'^package\s+([\w\.]+);', line):
          return package + '.' + short_class_name

  def _IgnoreDir(self, d):
    # Skip hidden directories.
    if d.startswith('.'):
      return True
    # Skip the "out" directory, as dealing with generated files is awkward.
    # We don't want paths like "out/Release/lib.java" in our DEPS files.
    # TODO(husky): We need some way of determining the "real" path to
    # a generated file -- i.e., where it would be in source control if
    # it weren't generated.
    if d.startswith('out') or d in ('xcodebuild', 'AndroidStudioDefault',
                                    'libassistant',):
      return True
    # Skip third-party directories.
    if d in ('third_party', 'ThirdParty'):
      return True
    return False

  def _ShouldIncludeRelPath(self, rel_path, target_filenames):
    """Returns whether rel_path (with '/' separators) should be prescanned."""
    if not rel_path.endswith('.java'):
      return False
    if (target_filenames is not None
        and posixpath.basename(rel_path) not in target_filenames):
      return False
    parts = rel_path.split('/')
    return not any(self._IgnoreDir(p) for p in parts[:-1])

  def _GetGitRepos(self, git_cmd):
    """Returns self._base_directory plus any non-ignored subrepositories."""
    repos = [self._base_directory]
    # Note: `gclient recurse` takes ~6.0s because it parses DEPS and runs across
    # all ~400 entries (mostly `third_party/` and CIPD packages that
    # `_IgnoreDir` skips). Reading `.gitmodules` takes ~3ms and lets us filter
    # out `_IgnoreDir` paths before spawning `git ls-files`.
    gitmodules = os.path.join(self._base_directory, '.gitmodules')
    if os.path.isfile(gitmodules):
      out = subprocess.check_output(
          [git_cmd, 'config', '--file', gitmodules, '--get-regexp', 'path'],
          stderr=subprocess.DEVNULL,
          text=True)
      for line in out.splitlines():
        parts = line.split(None, 1)
        if len(parts) != 2:
          continue
        rel_repo = parts[1]
        if any(self._IgnoreDir(p) for p in rel_repo.split('/')):
          continue
        repo_dir = os.path.join(self._base_directory, rel_repo)
        if os.path.exists(os.path.join(repo_dir, '.git')):
          repos.append(repo_dir)
    return repos

  def _PrescanFilesWalk(self, added_classset, target_filenames):
    """Fallback for non-git environments (e.g. Cog) using os.walk."""
    # Use a single ThreadPoolExecutor across the walk so file reads on slow
    # network filesystems (Cog) overlap with directory traversal rather than
    # creating and joining a new pool per directory.
    with concurrent.futures.ThreadPoolExecutor() as executor:
      futures_to_file = {}
      for root, dirs, files in os.walk(self._base_directory):
        # Skip unwanted subdirectories. TODO(husky): it would be better to do
        # this via the skip_child_includes flag in DEPS files. Maybe hoist this
        # prescan logic into checkdeps.py itself?
        # Modify dirs in-place with slice assignment to avoid recursing into
        # them.
        dirs[:] = [d for d in dirs if not self._IgnoreDir(d)]
        for f in files:
          if (f in target_filenames if target_filenames is not None
              else f.endswith('.java')):
            filepath = os.path.join(root, f)
            futures_to_file[executor.submit(self._PrescanFile, filepath)] = (
                filepath)
      for future in concurrent.futures.as_completed(futures_to_file):
        full_class_name = future.result()
        if full_class_name:
          self._ProcessFile(
              futures_to_file[future], full_class_name, added_classset)

  def _GetJavaFilesGit(self, target_filenames, added_imports):
    """Returns a list of .java file paths to prescan using git ls-files.

    Args:
      target_filenames: If not None, a set of basenames (e.g. {'Foo.java'})
          to restrict the returned files to.
      added_imports: The ((file_path, (import_line, ...)), ...) iterable from
          presubmit, used to ensure untracked local files are also included.
    """
    # Prefer `git ls-files` over `os.walk` when in a git checkout, as walking
    # the entire Chromium tree (~33,000 directories) is much slower than
    # reading the git index (~100-150ms). Note: `--others` is intentionally
    # omitted because scanning untracked directories across the tree takes
    # ~3.2s.
    java_files = []
    seen = set()
    git_cmd = 'git.bat' if os.name == 'nt' else 'git'
    repos = self._GetGitRepos(git_cmd)

    def list_repo(repo):
      return repo, subprocess.check_output(
          [git_cmd, '-C', repo, 'ls-files', '-z', '--', '*.java'],
          stderr=subprocess.DEVNULL,
          text=True)

    with concurrent.futures.ThreadPoolExecutor() as executor:
      repo_outputs = list(executor.map(list_repo, repos))
    for repo, out in repo_outputs:
      for rel_path in out.split('\0'):
        if not self._ShouldIncludeRelPath(rel_path, target_filenames):
          continue
        full_path = os.path.normpath(os.path.join(repo, rel_path))
        seen.add(os.path.normcase(os.path.abspath(full_path)))
        java_files.append(full_path)
    # `git ls-files` only lists tracked/staged files. Also include any
    # untracked files from `added_imports` (e.g. during `git cl presubmit
    # --force`).
    for filepath, _ in (added_imports or []):
      abs_path = os.path.abspath(filepath)
      norm_path = os.path.normcase(abs_path)
      if norm_path in seen or not os.path.isfile(filepath):
        continue
      rel_path = os.path.relpath(
          abs_path, self._base_directory).replace(os.sep, '/')
      if self._ShouldIncludeRelPath(rel_path, target_filenames):
        seen.add(norm_path)
        java_files.append(filepath)
    return java_files

  def _PrescanFiles(self, added_classset, added_imports=None):
    if not self._verbose and not added_classset:
      return
    # `_GetClassFullName` always constructs `<package>.<filename_without_java>`,
    # and when `not self._verbose`, `_ProcessFile` and `CheckLine` only ever
    # inspect entries in `_classmap` whose key is in `added_classset`. Thus we
    # only need to open and parse `.java` files whose basename matches the
    # short class name of an entry in `added_classset` (typically a few files
    # instead of all ~14,000 `.java` files in the tree).
    target_filenames = (
        None if self._verbose else
        {c.rsplit('.', 1)[-1] + '.java' for c in added_classset})
    if (os.getcwd().startswith('/google/cog/cloud')
        or not os.path.exists(os.path.join(self._base_directory, '.git'))):
      self._PrescanFilesWalk(added_classset, target_filenames)
      return

    java_files = self._GetJavaFilesGit(target_filenames, added_imports)
    with concurrent.futures.ThreadPoolExecutor() as executor:
      for filepath, full_class_name in zip(
          java_files, executor.map(self._PrescanFile, java_files)):
        if full_class_name:
          self._ProcessFile(filepath, full_class_name, added_classset)

  def _PrescanImportFiles(self, added_imports):
    """Build a set of fully-qualified class affected by this patch.

    Prescan imported files and build classset to collect full class names
    with package name. This includes both changed files as well as changed
    imports.

    Args:
      added_imports : ((file_path, (import_line, import_line, ...), ...)

    Return:
      A set of full class names with package name of imported files.
    """
    classset = set()
    for filepath, changed_lines in (added_imports or []):
      if not self.ShouldCheck(filepath):
        continue
      full_class_name = self._GetClassFullName(filepath)
      if full_class_name:
        classset.add(full_class_name)
      for line in changed_lines:
        found_item = self._EXTRACT_IMPORT_PATH.match(line)
        if found_item:
          classset.add(found_item.group(1))
    return classset

  def _PrescanFile(self, filepath):
    """Scans a file and returns its full class name, if any."""
    if self._verbose:
      print('Prescanning: ' + filepath)
    full_class_name = self._GetClassFullName(filepath)
    if not full_class_name and self._verbose:
      print('WARNING: no package definition found in %s' % filepath)
    return full_class_name

  def _ProcessFile(self, filepath, full_class_name, added_classset):
    """Populates _classmap based on the path and class name."""
    if full_class_name in self._classmap:
      if self._verbose or full_class_name in added_classset:
        if not any(re.match(i, filepath) for i in
                    self._allow_multiple_definitions):
          print('WARNING: multiple definitions of %s:' % full_class_name)
          print('    ' + filepath)
          print('    ' + self._classmap[full_class_name])
          print()
      # Prefer the public repo when multiple matches are found.
      if self._classmap[full_class_name].startswith(
          os.path.join(self._base_directory, 'clank')):
        self._classmap[full_class_name] = filepath
    else:
      self._classmap[full_class_name] = filepath

  def CheckLine(self, rules, line, filepath, fail_on_temp_allow=False):
    """Checks the given line with the given rule set.

    Returns a tuple (is_import, dependency_violation) where
    is_import is True only if the line is an import
    statement, and dependency_violation is an instance of
    results.DependencyViolation if the line violates a rule, or None
    if it does not.
    """
    found_item = self._EXTRACT_IMPORT_PATH.match(line)
    if not found_item:
      return False, None  # Not a match
    clazz = found_item.group(1)
    if clazz not in self._classmap:
      # Importing a class from outside the Chromium tree. That's fine --
      # it's probably a Java or Android system class.
      return True, None
    import_path = os.path.relpath(
        self._classmap[clazz], self._base_directory)
    # Convert Windows paths to Unix style, as used in DEPS files.
    import_path = import_path.replace(os.path.sep, '/')
    rule = rules.RuleApplyingTo(import_path, filepath)
    if (rule.allow == Rule.DISALLOW or
        (fail_on_temp_allow and rule.allow == Rule.TEMP_ALLOW)):
      return True, results.DependencyViolation(import_path, rule, rules)
    return True, None

  def CheckFile(self, rules, filepath):
    if self._verbose:
      print('Checking: ' + filepath)

    dependee_status = results.DependeeStatus(filepath)
    with open(filepath, encoding='utf-8') as f:
      for line in f:
        is_import, violation = self.CheckLine(rules, line, filepath)
        if violation:
          dependee_status.AddViolation(violation)
        if '{' in line:
          # This is code, so we're finished reading imports for this file.
          break

    return dependee_status

  @staticmethod
  def IsJavaFile(filepath):
    """Returns True if the given path ends in the extensions
    handled by this checker.
    """
    return os.path.splitext(filepath)[1] in JavaChecker.EXTENSIONS

  def ShouldCheck(self, file_path):
    """Check if the new import file path should be presubmit checked.

    Args:
      file_path: file path to be checked

    Return:
      bool: True if the file should be checked; False otherwise.
    """
    return self.IsJavaFile(file_path)
