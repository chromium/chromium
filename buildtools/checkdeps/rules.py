# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Base classes to represent dependency rules, used by checkdeps.py"""

import functools
import os
import re

# Sentinel telling "not cached yet" apart from any cached value.
_UNSET = object()


# The same specific_include_rules pattern (e.g. r'.*_unittest\.cc') appears in
# many DEPS files, so compile each distinct pattern only once.
@functools.lru_cache(maxsize=None)
def _CompileRegex(pattern):
  return re.compile(pattern)


class Rule(object):
  """Specifies a single rule for an include, which can be one of
  ALLOW, DISALLOW and TEMP_ALLOW.
  """

  # There is a Rule for every directory plus one for every DEPS entry. Naming
  # the attributes in __slots__ makes Python store them in fixed slots rather
  # than in a per-instance __dict__, which makes each instance much smaller.
  # _dir_slash is _dir + '/', precomputed for ChildOrMatch().
  __slots__ = ('allow', '_dir', '_dir_slash', '_dependent_dir', '_source')

  # These are the prefixes used to indicate each type of rule. These
  # are also used as values for self.allow to indicate which type of
  # rule this is.
  ALLOW = '+'
  DISALLOW = '-'
  TEMP_ALLOW = '!'

  def __init__(self, allow, directory, dependent_directory, source):
    self.allow = allow
    self._dir = directory
    self._dir_slash = directory + '/'
    self._dependent_dir = dependent_directory
    self._source = source

  def __str__(self):
    return '"%s%s" from %s.' % (self.allow, self._dir, self._source)

  def AsDependencyTuple(self):
    """Returns a tuple (allow, dependent dir, dependee dir) for this rule,
    which is fully self-sufficient to answer the question whether the dependent
    is allowed to depend on the dependee, without knowing the external
    context."""
    return self.allow, self._dependent_dir or '.', self._dir or '.'

  def ParentOrMatch(self, other):
    """Returns true if the input string is an exact match or is a parent
    of the current rule. For example, the input "foo" would match "foo/bar"."""
    return self._dir == other or self._dir.startswith(other + '/')

  def ChildOrMatch(self, other):
    """Returns true if the input string would be covered by this rule. For
    example, the input "foo/bar" would match the rule "foo"."""
    return self._dir == other or other.startswith(self._dir_slash)


class MessageRule(Rule):
  """A rule that has a simple message as the reason for failing,
  unrelated to directory or source.
  """

  # A subclass gets a __dict__ again unless it declares __slots__ too; list
  # only the attribute it adds.
  __slots__ = ('_reason',)

  def __init__(self, reason):
    super(MessageRule, self).__init__(Rule.DISALLOW, '', '', '')
    self._reason = reason

  def __str__(self):
    return self._reason


_NO_RULE_APPLYING = MessageRule('no rule applying.')


def ParseRuleString(rule_string, source):
  """Returns a tuple of a character indicating what type of rule this
  is, and a string holding the path the rule applies to.
  """
  if not rule_string:
    raise Exception(
      'The rule string "%s" is empty\nin %s' % (rule_string, source)
    )

  if rule_string[0] not in (Rule.ALLOW, Rule.DISALLOW, Rule.TEMP_ALLOW):
    raise Exception(
      'The rule string "%s" does not begin with a "+", "-" or "!".'
      % rule_string
    )

  # If a directory is specified in a DEPS file with a trailing slash, then it
  # will not match as a parent directory in Rule's [Parent|Child]OrMatch above.
  # Ban them.
  if rule_string[-1] == '/':
    raise Exception(
      'The rule string "%s" ends with a "/" which is not allowed.'
      ' Please remove the trailing "/".' % rule_string
    )

  return rule_string[0], rule_string[1:]


class Rules(object):
  """Sets of rules for files in a directory.

  By default, rules are added to the set of rules applicable to all
  dependee files in the directory.  Rules may also be added that apply
  only to dependee files whose filename (last component of their path)
  matches a given regular expression; hence there is one additional
  set of rules per unique regular expression.

  A Rules object stores only the rules declared for its own directory and
  links to the Rules of the parent directory, so creating one for a
  subdirectory is O(1) instead of copying every inherited rule. This relies
  on two invariants:
    * Rules are only added while a directory's DEPS file is applied, before
      any lookup is made on the object or on one of its children, so the
      lazily built lookup structures never go stale.
    * A directory's own rules are newer than, and take precedence over,
      everything it inherits.
  """

  # There is a Rules object for every directory; see Rule.__slots__.
  __slots__ = (
    '_parent',
    '_local_general_rules',
    '_local_specific_rules',
    '_deps_builder',
    '_general_dir_map',
    '_next_prio',
    '_general_cache',
    '_specific_compiled',
  )

  def __init__(self, deps_builder, parent=None):
    """Initializes the current rules with an empty rule list for all
    files, optionally inheriting from |parent| via zero-copy DAG sharing.
    """
    self._parent = parent
    # Local rules added to this directory node (most-recent first).
    self._local_general_rules = []
    # Local specific rules added to this directory node: regexp -> [Rule, ...].
    self._local_specific_rules = {}
    # DepsBuilder associated with this instance, used to evaluate
    # new_usages_require_review.
    self._deps_builder = deps_builder

    # Lookup structures, built on first use by the _Get*() methods below.
    # Rule directory -> (priority, Rule), for every general rule in effect.
    self._general_dir_map = None
    # Priority that the next rule added below this node would get.
    self._next_prio = 0
    # include_path -> Rule memo for general-only lookups.
    self._general_cache = None
    # ((compiled regexp, (Rule, ...)), ...), for every specific rule in effect.
    self._specific_compiled = None

  def CreateChild(self):
    """Creates a child Rules node inheriting from self in O(1)."""
    return Rules(self._deps_builder, parent=self)

  def _AllGeneralRules(self):
    """Returns all general rules from most-specific to root."""
    out = []
    cur = self
    while cur is not None:
      if cur._local_general_rules:
        out.extend(cur._local_general_rules)
      cur = cur._parent
    return out

  def _AllSpecificRules(self):
    """Returns dict of regexp -> list[Rule] (most-specific first)."""
    chain = []
    cur = self
    while cur is not None:
      if cur._local_specific_rules:
        chain.append(cur._local_specific_rules)
      cur = cur._parent
    if not chain:
      return {}
    if len(chain) == 1:
      return {k: list(v) for k, v in chain[0].items()}
    merged = {}
    # Merge from the root down: regexps keep the order in which they were
    # first declared, and where several levels declare the same regexp the
    # more deeply nested directory's rules come first (take precedence).
    for local_map in reversed(chain):
      for regexp, rule_list in local_map.items():
        if regexp in merged:
          merged[regexp] = rule_list + merged[regexp]
        else:
          merged[regexp] = list(rule_list)
    return merged

  def _GetGeneralDirMap(self):
    """Returns ({rule dir: (priority, Rule)}, next priority) for this node.

    The map holds every general rule in effect for this directory, inherited
    ones included. When several rules could apply to an include, the most
    recently added one wins (not the longest match), so each rule records
    its age as a priority: higher means newer. A rule naming the same
    directory as an older rule simply replaces it in the map.
    """
    if self._general_dir_map is not None:
      return self._general_dir_map, self._next_prio
    if self._parent is not None:
      parent_map, prio = self._parent._GetGeneralDirMap()
      dir_map = parent_map.copy()
    else:
      dir_map = {}
      prio = 0
    # _local_general_rules is stored newest-first; iterate in insertion order
    # so newer rules receive higher priority numbers.
    for rule in reversed(self._local_general_rules):
      dir_map[rule._dir] = (prio, rule)
      prio += 1
    self._general_dir_map = dir_map
    self._next_prio = prio
    return dir_map, prio

  def _GetSpecificCompiled(self):
    """Returns tuple of (compiled_regex, tuple[Rule, ...]) for this node."""
    if self._specific_compiled is not None:
      return self._specific_compiled
    if not self._local_specific_rules:
      # Nothing is declared here: share the parent's tuple as is.
      if self._parent is None:
        self._specific_compiled = ()
      else:
        self._specific_compiled = self._parent._GetSpecificCompiled()
      return self._specific_compiled
    merged = self._AllSpecificRules()
    self._specific_compiled = tuple(
      (_CompileRegex(regexp), tuple(rule_list))
      for regexp, rule_list in merged.items()
    )
    return self._specific_compiled

  def __str__(self):
    result = [
      'Rules = {\n    (apply to all files): [\n%s\n    ],'
      % '\n'.join('      %s' % x for x in self._AllGeneralRules())
    ]
    for regexp, rule_list in self._AllSpecificRules().items():
      result.append(
        '    (limited to files matching %s): [\n%s\n    ]'
        % (regexp, '\n'.join('      %s' % x for x in rule_list))
      )
    result.append('  }')
    return '\n'.join(result)

  def _FindFirstAncestorThatRequiresReview(self, include_path):
    return self._deps_builder.FindFirstAncestorThatRequiresReview(include_path)

  def AsDependencyTuples(self, include_general_rules, include_specific_rules):
    """Returns a list of tuples (allow, dependent dir, dependee dir) for the
    specified rules (general/specific). Currently only general rules are
    supported."""

    def AddDependencyTuplesImpl(deps, rule_list, extra_dependent_suffix=''):
      for rule in rule_list:
        (allow, dependent, dependee) = rule.AsDependencyTuple()
        tup = (allow, dependent + extra_dependent_suffix, dependee)
        deps.add(tup)

    deps = set()
    if include_general_rules:
      AddDependencyTuplesImpl(deps, self._AllGeneralRules())
    if include_specific_rules:
      for regexp, rule_list in self._AllSpecificRules().items():
        AddDependencyTuplesImpl(deps, rule_list, '/' + regexp)
    return deps

  def AddRule(self, rule_string, dependent_dir, source, dependee_regexp=None):
    """Adds a rule for the given rule string.

    Args:
      rule_string: The include_rule string read from the DEPS file to apply.
      source: A string representing the location of that string (filename, etc.)
              so that we can give meaningful errors.
      dependent_dir: The directory to which this rule applies.
      dependee_regexp: The rule will only be applied to dependee files
                       whose filename (last component of their path)
                       matches the expression. None to match all
                       dependee files.
    """
    rule_type, rule_dir = ParseRuleString(rule_string, source)
    rule = Rule(rule_type, rule_dir, dependent_dir, source)

    # Rules must not be added once a lookup was made (see the class comment):
    # the lookup structures of this object and of its children would be stale.
    if not dependee_regexp:
      assert self._general_dir_map is None and self._general_cache is None
      self._local_general_rules.insert(0, rule)
    else:
      assert self._specific_compiled is None
      rules_to_update = self._local_specific_rules.get(dependee_regexp)
      if rules_to_update is None:
        self._local_specific_rules[dependee_regexp] = [rule]
      else:
        rules_to_update.insert(0, rule)

  def GetSpecificRulesForFile(self, dependee_filename):
    """Returns the specific rules for a file named |dependee_filename|.

    Checkers call this once per file and pass the result to
    RuleApplyingToWithSpecific() for each include, rather than matching
    every regexp again for every include. Returns None if no regexp matches.
    """
    compiled = self._GetSpecificCompiled()
    if not compiled:
      return None
    matched = None
    for regex, rule_list in compiled:
      if regex.fullmatch(dependee_filename):
        if matched is None:
          matched = list(rule_list)
        else:
          matched.extend(rule_list)
    return matched

  def _MatchGeneralNoReview(self, include_path):
    """Returns the general rule applying to |include_path|.

    Only valid when no new_usages_require_review directory is involved. A
    rule applies if its directory is |include_path| itself or one of its
    ancestors, so rather than testing every rule this probes the map once
    per path component and keeps the newest hit. Results are memoized, as
    the files of a directory tend to include the same headers.
    """
    cache = self._general_cache
    if cache is None:
      cache = {}
      self._general_cache = cache
    cached = cache.get(include_path, _UNSET)
    if cached is not _UNSET:
      return cached

    dir_map, _ = self._GetGeneralDirMap()
    best_prio = -1
    best_rule = None

    entry = dir_map.get(include_path)
    if entry is not None:
      best_prio, best_rule = entry

    idx = include_path.rfind('/')
    while idx != -1:
      prefix = include_path[:idx]
      entry = dir_map.get(prefix)
      if entry is not None and entry[0] > best_prio:
        best_prio, best_rule = entry
      idx = include_path.rfind('/', 0, idx)

    result = best_rule if best_rule is not None else _NO_RULE_APPLYING
    cache[include_path] = result
    return result

  def RuleApplyingToWithSpecific(self, include_path, matched_specific_rules):
    """Like RuleApplyingTo(), given GetSpecificRulesForFile()'s result.

    Specific rules take precedence over general ones; within each group the
    most recently added matching rule wins.
    """
    if not matched_specific_rules:
      cache = self._general_cache
      if cache is not None:
        cached = cache.get(include_path, _UNSET)
        if cached is not _UNSET:
          return cached

    review_parent = self._FindFirstAncestorThatRequiresReview(include_path)
    # Common case: the include is not under a new_usages_require_review
    # directory.
    if review_parent is None:
      if matched_specific_rules:
        for rule in matched_specific_rules:
          if rule.ChildOrMatch(include_path):
            return rule
      return self._MatchGeneralNoReview(include_path)

    # The include is under a directory requiring review. Only a rule naming
    # that directory or something below it may allow the include, which the
    # prefix map cannot express, so test the rules one by one, newest first.
    almost_match = None
    if matched_specific_rules:
      for rule in matched_specific_rules:
        if rule.ChildOrMatch(include_path):
          if rule.ParentOrMatch(review_parent):
            return rule
          almost_match = rule
    result = None
    cur = self
    while cur is not None and result is None:
      for rule in cur._local_general_rules:
        if rule.ChildOrMatch(include_path):
          if rule.ParentOrMatch(review_parent):
            result = rule
            break
          almost_match = rule
      cur = cur._parent
    if result is None:
      if almost_match:
        result = MessageRule(
          f'no rule applying for directory "{review_parent}", '
          'which is marked as new_usages_require_review=True.'
        )
      else:
        result = _NO_RULE_APPLYING
    if not matched_specific_rules:
      if self._general_cache is None:
        self._general_cache = {}
      self._general_cache[include_path] = result
    return result

  def RuleApplyingTo(self, include_path, dependee_path):
    """Returns the rule that applies to |include_path| for a dependee
    file located at |dependee_path|.
    """
    matched_specific = self.GetSpecificRulesForFile(
      os.path.basename(dependee_path)
    )
    return self.RuleApplyingToWithSpecific(include_path, matched_specific)
