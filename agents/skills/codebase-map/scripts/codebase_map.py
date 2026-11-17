#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts a structural map of a Chromium directory for visualization.

Collects deterministic facts about a directory: the file tree, resolved OWNERS
(including inheritance, `per-file` rules, `set noparent` and `file://`
includes), DIR_METADATA, recent git activity (authors *and* reviewers, from
`Reviewed-by:` trailers), and leading header comments.

The output JSON is consumed by `templates/map.html`. Descriptions in the JSON
(`summary` on the root and `description` on nodes) are intentionally left empty
for an agent to fill in; see SKILL.md.

Usage:
  codebase_map.py extract <dir> [--since 6.months] [-o map.json]
  codebase_map.py targets map.json [--hot N]   # what the agent should describe
  codebase_map.py annotate map.json annotations.json [-o map.json]
  codebase_map.py render map.json [-o map.html]
  codebase_map.py build <dir> [-o map.html]   # extract + render, no agent step
  codebase_map.py build <dir> --serve         # ...and open it
  codebase_map.py serve map.html [--port N]   # serve an existing map

When `-o` is omitted for `render`/`build`, the HTML goes to
`<repo>/out/codebase_map/<dir>.html` (each `/` in the directory becomes
`__`), which is gitignored.

`serve` exists because `file://` links don't work when the script runs on a
remote machine (cloudtop, SSH). It binds a local HTTP server that serves only
the `.html` maps in the output directory and prints a
`http://localhost:<port>/...` URL; remote IDEs auto-detect that and offer to
forward the port, and on a local machine the default browser is opened. A
second `serve` of the same directory reuses the running server, and servers
exit on their own after an idle timeout. `--proxy` additionally listens on all
interfaces for browser-side proxies such as
`http://<cloudtop>.c.googlers.com:<port>/`.
"""

import argparse
import collections
import contextlib
import copy
import datetime
import fnmatch
import hashlib
import http.client
import http.server
import json
import os
import platform
import re
import socket
import subprocess
import sys
import threading
import time
import urllib.parse
import webbrowser

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_TEMPLATE = os.path.join(_SCRIPT_DIR, os.pardir, 'templates', 'map.html')
_PREFERRED_PORT = 8765
# Bump when the JSON shape changes in a way the template depends on.
_SCHEMA_VERSION = 2
# A served map shuts itself down after this long without a request, so servers
# left running in the background don't pile up.
_DEFAULT_IDLE_TIMEOUT_MIN = 120
# Answers "is a codebase-map server already serving this directory?".
_PROBE_PATH = '/__codebase_map__'
_LOCAL_HOSTS = ('localhost', '127.0.0.1', '::1')

_IGNORED_DIRS = {'.git', '__pycache__', 'node_modules'}
_MAX_HEADER_COMMENT_LINES = 6
_MAX_RECENT_COMMITS = 40
_MAX_FILE_RECENT_COMMITS = 5
_MAX_TOP_PEOPLE_PER_PATH = 5
_MAX_FILE_INCLUDE_DEPTH = 4
# Gerrit annotation: suggest this owner only when nobody else can approve.
_LAST_RESORT = '#{LAST_RESORT_SUGGESTION}'
_DEFAULT_HOT_FILES = 15
_DEFAULT_OUT_DIR = os.path.join('out', 'codebase_map')

# Gerrit writes one of these per approving reviewer into the commit message.
_REVIEWED_BY_RE = re.compile(r'^Reviewed-by:\s*(.*?)\s*<([^>]+)>\s*$', re.M)


# ---------------------------------------------------------------------------
# Helpers


class MapError(Exception):
    """A problem to report to the user; `main` prints it and exits."""


def _run_git(repo_root, args):
    # git prints paths and commit text as UTF-8 on every platform. Decoding
    # with the locale's encoding instead (cp1252 on Windows) would garble
    # non-ASCII file names so they no longer match the tree.
    return subprocess.run(
        ['git', '-C', repo_root] + args,
        check=True,
        capture_output=True,
        encoding='utf-8',
        errors='replace',
    ).stdout


def _git_dir_output(text):
    """A directory printed by `git rev-parse`, in this platform's form.

    Git for Windows prints `D:/src/chromium`; everything else here builds
    paths with `os.path`, so convert to `D:\\src\\chromium` to match.
    """
    text = text.strip()
    return os.path.normpath(text) if text else ''


def _find_repo_root(path):
    """The outermost repository containing `path`.

    Chromium pulls `v8/`, `third_party/skia/` and friends in as git
    submodules. Mapping one of those should still use Chromium's `src/` for
    output paths, codesearch links and the OWNERS above the submodule, so this
    walks up through superprojects. Git commands that need the submodule's
    own history use `_find_git_root` instead.
    """
    root = _find_git_root(path)
    while True:
        try:
            parent = subprocess.run(
                [
                    'git',
                    '-C',
                    root,
                    'rev-parse',
                    '--show-superproject-working-tree',
                ],
                check=True,
                capture_output=True,
                encoding='utf-8',
            ).stdout
        except subprocess.CalledProcessError:
            parent = ''
        parent = _git_dir_output(parent)
        if not parent:
            return root
        root = parent


def _find_git_root(path):
    """The innermost repository containing `path`."""
    try:
        return _git_dir_output(
            subprocess.run(
                ['git', '-C', path, 'rev-parse', '--show-toplevel'],
                check=True,
                capture_output=True,
                encoding='utf-8',
            ).stdout
        )
    except subprocess.CalledProcessError:
        raise MapError(f'{path} is not inside a git repository') from None


def _git_scope(repo_root, rel_root, git_root):
    """Maps a repo-relative directory into `git_root`.

    Returns (pathspec, prefix): run git in `git_root` with `pathspec`, then
    join `prefix` onto the paths it prints to get repo-relative paths again.
    """
    prefix = os.path.relpath(git_root, repo_root).replace(os.sep, '/')
    prefix = '' if prefix == '.' else prefix
    pathspec = os.path.relpath(
        os.path.join(repo_root, rel_root), git_root
    ).replace(os.sep, '/')
    return pathspec, prefix


def _join_prefix(prefix, path):
    return f'{prefix}/{path}' if prefix else path


def _classify(name, rel_dir):
    """Returns a coarse role for a file, used for grouping and coloring."""
    if name in ('OWNERS', 'DIR_METADATA'):
        return 'meta'
    if name in ('BUILD.gn', 'DEPS') or name.endswith(('.gn', '.gni')):
        return 'build'
    if name.endswith('.md'):
        return 'docs'
    stem = name.rsplit('.', 1)[0]
    last_dir = rel_dir.rsplit('/', 1)[-1]
    if (
        re.search(
            r'(_unittest|_browsertest|_interactive_uitest|_test|'
            r'_perftest|_fuzzer|_unittests|_testing)$',
            stem,
        )
        or stem.startswith(('mock_', 'fake_', 'test_'))
        or '/test/' in f'/{rel_dir}/'
        or last_dir in ('test', 'tests')
        or last_dir.endswith(('_test', '_tests'))
    ):
        return 'test'
    if name.endswith('.mojom'):
        return 'mojom'
    if name.endswith(('.h', '.hpp')):
        if rel_dir.split('/')[-1] == 'public' or '/public/' in f'/{rel_dir}/':
            return 'public'
        return 'header'
    if name.endswith(('.cc', '.cpp', '.mm', '.c')):
        return 'impl'
    if name.endswith(('.java', '.kt')):
        return 'impl'
    if name.endswith(('.ts', '.js', '.html', '.css')):
        return 'webui'
    if name.endswith('.py'):
        return 'script'
    return 'other'


def _count_lines(path):
    """Line count, or 0 for unreadable or binary files.

    A NUL byte in the first 8 KiB is treated as binary, the same heuristic
    git uses.
    """
    try:
        with open(path, 'rb') as f:
            if b'\0' in f.read(8192):
                return 0
            f.seek(0)
            return sum(1 for _ in f)
    except OSError:
        return 0


_LICENSE_RE = re.compile(r'copyright|license|found in the LICENSE', re.I)
_DECL_RE = re.compile(
    r'^(?:template\s*<.*>\s*)?'
    r'(?:class|struct|enum(?:\s+class)?|union)\s+'
    r'(?:[A-Z_]+\s+)?[A-Za-z_]\w*'
)
_SKIP_COMMENT_PREFIXES = (
    'TODO',
    'FIXME',
    'NOLINT',
    'clang-format',
    'IWYU',
    'Forward decl',
)


def _header_comment(path):
    """Returns the first descriptive comment block in a source file.

    Prefers a file-level comment (one that sits between the license header
    and the first `#include`/code). Failing that, returns the comment attached
    to the first `class`/`struct`/`enum` declaration, which is where Chromium
    style puts the description of a one-class header.
    """
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            lines = f.read(30000).splitlines()
    except OSError:
        return ''
    block = []
    seen_code = False  # Anything after the license that isn't a comment.
    in_license = True

    def text_of(block):
        return ' '.join(block[:_MAX_HEADER_COMMENT_LINES])

    for line in lines[:400]:
        s = line.strip()
        if s.startswith('//'):
            text = s[2:].strip()
            if in_license and (_LICENSE_RE.search(text) or not text):
                continue
            in_license = False
            if not text:
                continue
            if not block and text.startswith(_SKIP_COMMENT_PREFIXES):
                continue
            block.append(text)
            continue
        in_license = False
        if not s:
            # A blank line closes a comment block. If no code has appeared
            # yet, this was a file-level description: done.
            if block and not seen_code:
                return text_of(block)
            block = []
            continue
        if s.startswith(('#ifndef', '#define', '#include', '#pragma')):
            if block and not seen_code:
                return text_of(block)
            seen_code = True
            block = []
            continue
        # Real code. A comment directly above a type declaration is that
        # type's description; anything else (namespace, forward decl, using)
        # just resets.
        seen_code = True
        if block and _DECL_RE.match(s):
            return text_of(block)
        block = []
    return ''


# ---------------------------------------------------------------------------
# OWNERS


class OwnersIndex:
    """Parses and resolves OWNERS files for paths under a repo root."""

    def __init__(self, repo_root):
        self.repo_root = repo_root
        self._cache = {}
        # Repo-relative paths of the files being expanded, outermost first.
        self._expanding = []

    def parse(self, rel_dir, _depth=0):
        """Parses `<rel_dir>/OWNERS`. Returns a dict or None if absent.

        Result shape:
          {
            'path': 'a/b/OWNERS', 'owners': [...], 'noparent': bool,
            'per_file': [{'pattern': str, 'owners': [...], 'noparent': bool,
                          'last_resort': [...], 'unresolved': [...]}],
            'includes': ['x/y/OWNERS', ...], 'comments': {email: comment},
            'last_resort': [email, ...],  # tagged #{LAST_RESORT_SUGGESTION}
            'unresolved': ['file://x/OWNERS', ...],  # includes not followed
            'ignored': [line, ...],       # unsupported directives
          }

        `last_resort` and `unresolved` are scoped to the entry that carries
        them: someone tagged last-resort in a parent OWNERS is still a
        regular owner where a nearer file lists them without the tag.

        Only top-level parses (`_depth == 0`) are cached. A parse reached
        through `file://` includes may have been cut off by the include-depth
        limit or by an include cycle, so caching it would make a later direct
        lookup depend on traversal order. Nested parses still reuse a cached
        top-level result, which is never truncated more than a fresh one
        would be.
        """
        if rel_dir in self._cache:
            return self._cache[rel_dir]
        if _depth:
            return self._parse_uncached(rel_dir, _depth)
        # The file itself heads the include stack, so an include that leads
        # back to it is recognised as a cycle.
        self._expanding.append(f'{rel_dir}/OWNERS' if rel_dir else 'OWNERS')
        try:
            info = self._parse_uncached(rel_dir, 0)
        finally:
            self._expanding.pop()
        self._cache[rel_dir] = info
        return info

    def _parse_uncached(self, rel_dir, depth):
        owners_path = os.path.join(self.repo_root, rel_dir, 'OWNERS')
        if not os.path.isfile(owners_path):
            return None
        info = {
            # Always `/`-separated, like every other path in the map.
            'path': f'{rel_dir}/OWNERS' if rel_dir else 'OWNERS',
            'owners': [],
            'noparent': False,
            'per_file': [],
            'includes': [],
            'comments': {},
            'last_resort': [],
            'unresolved': [],
            'ignored': [],
        }
        per_file = collections.OrderedDict()
        pending_comment = []
        with open(owners_path, encoding='utf-8', errors='replace') as f:
            for raw in f:
                stripped = raw.strip()
                if not stripped:
                    pending_comment = []
                    continue
                if stripped.startswith('#'):
                    pending_comment.append(stripped.lstrip('# ').strip())
                    continue
                # Trailing comments (`set noparent  # why`) must not leak into
                # directive matching or `file://` paths.
                line = _strip_comment(stripped)
                comment = ' '.join(pending_comment)
                pending_comment = []
                tagged = _LAST_RESORT in stripped
                if line == 'set noparent':
                    info['noparent'] = True
                    continue
                if line.startswith('per-file '):
                    rule, _, rhs = line[len('per-file ') :].partition('=')
                    patterns = [p.strip() for p in rule.split(',')]
                    rhs = rhs.strip()
                    for pattern in patterns:
                        entry = per_file.setdefault(
                            pattern,
                            {
                                'pattern': pattern,
                                'owners': [],
                                'noparent': False,
                                'comment': comment,
                                'last_resort': [],
                                'unresolved': [],
                            },
                        )
                        if rhs == 'set noparent':
                            entry['noparent'] = True
                            continue
                        # RHS may list several owners: a@x.org,b@x.org
                        for tok in rhs.split(','):
                            expansion = self._expand(tok, rel_dir, depth)
                            _merge_expansion(entry, expansion, tagged)
                    continue
                token = _include_to_file_token(line, rel_dir)
                is_include = token.startswith('file://')
                expansion = self._expand(token, rel_dir, depth)
                if not is_include and not expansion.owners:
                    # Not an email, `*` or include: an unsupported directive.
                    # Keep it out of the owner list so the planner never
                    # offers it as an approver.
                    info['ignored'].append(line)
                    continue
                for o in _merge_expansion(info, expansion, tagged):
                    if comment:
                        info['comments'][o] = comment
                if is_include:
                    info['includes'].append(token[len('file://') :])
        info['per_file'] = list(per_file.values())
        return info

    def _expand(self, token, rel_dir, depth):
        """Expands a single owners token into an `_Expansion`.

        Anything that isn't an email, `*` or an include expands to nothing,
        so unsupported directives never show up as owners. An include that
        can't be followed (missing file, or nested deeper than
        `_MAX_FILE_INCLUDE_DEPTH`) is reported as `unresolved` rather than as
        an owner, so it is shown and flagged but never suggested as a
        reviewer. An include of a file that is already being expanded (a
        cycle) adds nothing and is not an error.
        """
        token = _strip_comment(token)
        if not token:
            return _Expansion()
        # `per-file x=file: //a/OWNERS` and friends reach here unnormalised.
        token = _include_to_file_token(token, rel_dir)
        if not token.startswith('file://'):
            return _Expansion([token] if _is_owner_token(token) else [])
        inc = token[len('file://') :]
        if inc in self._expanding:
            # A cycle (a includes b, b includes a): what this file lists is
            # already being collected further up the stack.
            return _Expansion()
        if depth >= _MAX_FILE_INCLUDE_DEPTH:
            return _Expansion(unresolved=[token])
        self._expanding.append(inc)
        try:
            return self._expand_include(token, inc, depth)
        finally:
            self._expanding.pop()

    def _expand_include(self, token, inc, depth):
        """Reads the owners of the included file `inc` (repo-relative)."""
        inc_dir, inc_name = os.path.split(inc)
        if inc_name == 'OWNERS':
            sub = self.parse(inc_dir, depth + 1)
            if not sub:
                return _Expansion(unresolved=[token])
            return _Expansion(
                sub['owners'], sub['last_resort'], sub['unresolved']
            )
        inc_path = os.path.join(self.repo_root, inc)
        if not os.path.isfile(inc_path):
            return _Expansion(unresolved=[token])
        out = _Expansion()
        with open(inc_path, encoding='utf-8', errors='replace') as f:
            for raw in f:
                line = _strip_comment(raw)
                if (
                    not line
                    or line == 'set noparent'
                    or line.startswith('per-file')
                ):
                    continue
                sub = self._expand(line, inc_dir, depth + 1)
                if _LAST_RESORT in raw:
                    sub.last_resort = list(sub.owners)
                out.extend(sub)
        return out

    def all_parsed(self):
        """Every OWNERS file parsed so far (dirs without one excluded)."""
        return [info for info in self._cache.values() if info]

    def chain(self, rel_dir):
        """Returns the OWNERS files that apply to rel_dir, nearest first.

        Stops at the first `set noparent` or at the repo root.
        """
        out = []
        cur = rel_dir
        while True:
            info = self.parse(cur)
            if info:
                out.append(info)
                if info['noparent']:
                    break
            if not cur:
                break
            cur = os.path.dirname(cur)
        return out

    def resolve_dir(self, rel_dir):
        """Returns (owners, source_path) for a directory.

        Uses the same "nearest OWNERS file that lists people" rule as
        `resolve_file`, so a directory whose OWNERS has only `per-file` rules
        reports its parent's owners rather than none.
        """
        for info in self.chain(rel_dir):
            if info['owners']:
                return info['owners'], info['path']
        return [], None

    def resolve_file(self, rel_file):
        """Computes who can approve a file.

        Returns {'owners', 'rules', 'chain', 'inherited_rules',
        'last_resort'}. `owners` is the *nearest* approver set: matching
        `per-file` rules plus the closest OWNERS file that lists people.
        `rules` explains where each came from. `chain` lists every OWNERS
        file whose directory-level owners can approve the file, nearest first
        (useful context, but rarely who you'd actually ask), and
        `inherited_rules` the matching `per-file` rules found above the
        nearest set, whose owners can approve as well. `last_resort` is the
        subset of `owners` that every contributing rule tags
        #{LAST_RESORT_SUGGESTION}.

        `per-file` scoping follows Gerrit's code-owners `find-owners` backend
        (FIND_OWNERS_GLOB): a pattern in `a/OWNERS` is matched against the
        file's path relative to `a/`, implicitly prefixed with `{**/,}`, so
        `per-file *.mojom` in `a/` also covers `a/sub/x.mojom`. Every OWNERS
        file on the way up is checked for matching `per-file` rules. Those
        found up to and including the closest file that lists people form the
        nearest set; later ones are `inherited_rules`.

        A matching `per-file ... set noparent` ends the walk wherever it is
        found: that rule's owners can approve, but not the directory owners
        of the same OWNERS file and nothing above it. OWNERS files nearer to
        the file are unaffected.
        """
        rel_dir = os.path.dirname(rel_file)
        rules = []
        inherited_rules = []
        chain = []
        nearest_found = False
        for info in self.chain(rel_dir):
            owners_dir = os.path.dirname(info['path'])
            rel_to_owners = rel_file
            if owners_dir:
                rel_to_owners = os.path.relpath(rel_file, owners_dir)
            rel_to_owners = rel_to_owners.replace(os.sep, '/')
            matched = [
                {
                    'source': info['path'],
                    'pattern': pf['pattern'],
                    'owners': pf['owners'],
                    'noparent': pf['noparent'],
                    'comment': pf.get('comment', ''),
                    'last_resort': pf['last_resort'],
                    'unresolved': pf['unresolved'],
                }
                for pf in info['per_file']
                if _per_file_matches(rel_to_owners, pf['pattern'])
            ]
            (inherited_rules if nearest_found else rules).extend(matched)
            if any(rule['noparent'] for rule in matched):
                break
            if not nearest_found:
                if info['owners'] or info['unresolved']:
                    rules.append(
                        {
                            'source': info['path'],
                            'pattern': None,
                            'owners': info['owners'],
                            'noparent': info['noparent'],
                            'last_resort': info['last_resort'],
                            'unresolved': info['unresolved'],
                        }
                    )
                nearest_found = bool(info['owners'])
            chain.append(info['path'])
        owners = []
        regular = set()
        for rule in rules:
            for o in rule['owners']:
                if o not in owners:
                    owners.append(o)
                if o not in rule['last_resort']:
                    regular.add(o)
        return {
            'owners': owners,
            'rules': rules,
            'chain': chain,
            'inherited_rules': inherited_rules,
            'last_resort': [o for o in owners if o not in regular],
        }


class _Expansion:
    """What one OWNERS token stands for.

    `owners` are emails or `*`, `last_resort` the subset tagged
    #{LAST_RESORT_SUGGESTION} where they were listed, and `unresolved` the
    `file://` includes that could not be followed.
    """

    def __init__(self, owners=(), last_resort=(), unresolved=()):
        self.owners = list(owners)
        self.last_resort = list(last_resort)
        self.unresolved = list(unresolved)

    def extend(self, other):
        self.owners.extend(other.owners)
        self.last_resort.extend(other.last_resort)
        self.unresolved.extend(other.unresolved)


def _merge_expansion(target, expansion, tagged):
    """Adds `expansion` to an OWNERS file dict or one of its per-file entries.

    `tagged` says the line itself carried #{LAST_RESORT_SUGGESTION}, which
    applies to everything the line expands to. Returns the owners that were
    not already listed.
    """
    added = []
    for o in expansion.owners:
        if o not in target['owners']:
            target['owners'].append(o)
            added.append(o)
    last_resort = expansion.owners if tagged else expansion.last_resort
    for key, values in (
        ('last_resort', last_resort),
        ('unresolved', expansion.unresolved),
    ):
        for v in values:
            if v not in target[key]:
                target[key].append(v)
    return added


def _strip_comment(text):
    """Drops a trailing `# comment` and surrounding whitespace."""
    return text.split('#', 1)[0].strip()


def _is_owner_token(token):
    """True for an email or `*`, the only literal owner tokens Gerrit takes."""
    if token == '*':
        return True
    return bool(re.fullmatch(r'[^\s@,=]+@[^\s@,=]+', token))


def _include_to_file_token(line, rel_dir):
    """Rewrites an include directive as a canonical `file://<repo path>`.

    Accepts Gerrit's `include <path>` and code-owners' `file:<path>` (with or
    without a space, so `file: //a/OWNERS` and `file://a/OWNERS` are the same).
    A leading `/` or `//` is relative to the repo root; anything else is
    relative to the directory of the OWNERS file being parsed. Other lines,
    and cross-project `file:project:branch:path` references, pass through.
    Only the included file's owner lines are used (see SKILL.md Limits).
    """
    if line.startswith('include '):
        path = line[len('include ') :].strip()
    elif line.startswith('file:'):
        path = line[len('file:') :].strip()
        if ':' in path:
            return line
    else:
        return line
    if path.startswith('/'):
        path = path.lstrip('/')
    else:
        path = os.path.join(rel_dir, path)
    return 'file://' + os.path.normpath(path).replace(os.sep, '/')


def _per_file_matches(rel_path, pattern):
    """FIND_OWNERS_GLOB match of a `per-file` pattern.

    `rel_path` is relative to the OWNERS file's directory. The pattern is
    matched against the trailing path components, segment by segment, so `*`
    never crosses a `/` and `foo/*.cc` only matches a `foo` directory.
    """
    parts = rel_path.split('/')
    pat_parts = pattern.strip('/').split('/')
    if len(pat_parts) > len(parts):
        return False
    tail = parts[len(parts) - len(pat_parts) :]
    return all(fnmatch.fnmatchcase(p, q) for p, q in zip(tail, pat_parts))


# ---------------------------------------------------------------------------
# DIR_METADATA


_MD_OPEN_RE = re.compile(r'(\w+)\s*:?\s*\{')
_MD_FIELD_RE = re.compile(r'(\w+)\s*:\s*("(?:[^"\\]|\\.)*"|[^\s{}]+)')


def _parse_dir_metadata(path, repo_root=None):
    """Minimal textproto reader for the common DIR_METADATA fields.

    Handles blocks spanning lines or sitting on one line
    (`monorail { component: "X" }`). With `repo_root`, `mixins:` entries
    (`//path/COMMON_METADATA`) are read and merged underneath this file's own
    values, the way `dirmd` applies them. Mixins of mixins are not followed;
    `dirmd` doesn't allow them either.
    """
    out = _parse_textproto(path)
    if out is None or not repo_root:
        return out
    mixins = out.get('mixins', [])
    if isinstance(mixins, str):
        mixins = [mixins]
    for mixin in mixins:
        mixin_path = os.path.join(repo_root, mixin.lstrip('/'))
        mixin_data = _parse_textproto(mixin_path)
        if mixin_data:
            mixin_data.pop('mixins', None)
            _merge_missing(out, mixin_data)
    return out


def _parse_textproto(path):
    if not os.path.isfile(path):
        return None
    out = {}
    stack = [out]
    with open(path, encoding='utf-8', errors='replace') as f:
        for raw in f:
            rest = raw.split('#', 1)[0].strip()
            while rest:
                m = _MD_OPEN_RE.match(rest)
                if m:
                    child = {}
                    stack[-1][m.group(1)] = child
                    stack.append(child)
                elif rest.startswith('}'):
                    if len(stack) > 1:
                        stack.pop()
                    m = re.match(r'\}', rest)
                else:
                    m = _MD_FIELD_RE.match(rest)
                    if not m:
                        break  # Unrecognised syntax: skip the rest of line.
                    key, val = m.group(1), m.group(2).strip('"')
                    cur = stack[-1]
                    if key in cur:
                        if not isinstance(cur[key], list):
                            cur[key] = [cur[key]]
                        cur[key].append(val)
                    else:
                        cur[key] = val
                rest = rest[m.end() :].lstrip(' \t;,')
    return out


def _merge_missing(dst, src):
    """Copies keys from `src` that `dst` lacks, recursing into dicts."""
    for key, val in src.items():
        if key not in dst:
            dst[key] = val
        elif isinstance(dst[key], dict) and isinstance(val, dict):
            _merge_missing(dst[key], val)


def _inherited_dir_metadata(repo_root, rel_dir, cache):
    """DIR_METADATA for `rel_dir` with parent directories' values filled in.

    `dirmd` computes a directory's metadata the same way: the nearest file
    that sets a field wins, and a file's own mixins rank just below it.
    Returns (metadata, source): `source` is the repo-relative path of the
    nearest DIR_METADATA, or (None, None) when no directory up to the repo
    root has one. `cache` maps a directory to its result, so each file is
    read once per extraction.
    """
    if rel_dir in cache:
        return cache[rel_dir]
    own = _parse_dir_metadata(
        os.path.join(repo_root, rel_dir, 'DIR_METADATA'), repo_root
    )
    inherited = (None, None)
    if rel_dir:
        inherited = _inherited_dir_metadata(
            repo_root, os.path.dirname(rel_dir), cache
        )
    if own is None:
        result = inherited
    else:
        # Copy before merging: the parsed dict's nested blocks must not pick
        # up a parent's fields in place.
        merged = copy.deepcopy(own)
        if inherited[0]:
            _merge_missing(merged, inherited[0])
        source = f'{rel_dir}/DIR_METADATA' if rel_dir else 'DIR_METADATA'
        result = (merged, source)
    cache[rel_dir] = result
    return result


def _component(md):
    """The bug component to show for parsed DIR_METADATA, or ''.

    Prefers the public Buganizer component (what most Chromium directories
    use now), then the internal one, then the deprecated Monorail component.
    """
    if not md:
        return ''
    for block, key in (
        ('buganizer_public', 'component_id'),
        ('buganizer', 'component_id'),
        ('monorail', 'component'),
    ):
        section = md.get(block)
        val = section.get(key) if isinstance(section, dict) else None
        if val:
            return val if isinstance(val, str) else val[0]
    return ''


# ---------------------------------------------------------------------------
# Git activity


def _git_activity(repo_root, rel_root, since, git_root=None):
    """Returns per-path activity and the recent commit list for rel_root.

    The per-path map covers both files and directories; a directory's counts
    reflect distinct commits touching anything beneath it (not a sum over
    files, which would double count multi-file commits).

    Reviewers come from Gerrit's `Reviewed-by:` trailers, so "reviews" means
    approvals that actually landed, not every CL someone was CC'd on.

    `git_root` is the repository whose history to read when rel_root lives in
    a submodule; paths in the result are still relative to `repo_root`.
    """
    # Header fields are separated by \x1f; the body follows on its own lines
    # and is terminated by \x1d so it can't be confused with the file list.
    # -z: file names are NUL-terminated and printed as they are. Without it
    # git C-quotes any path with non-ASCII bytes ("a/\303\251.cc").
    # Full hashes (%H): crrev.com treats an all-digit abbreviation as a commit
    # position, and short hashes can become ambiguous. The UI shortens them
    # for display only.
    git_root = git_root or repo_root
    pathspec, prefix = _git_scope(repo_root, rel_root, git_root)
    log = _run_git(
        git_root,
        [
            'log',
            f'--since={since}',
            '--no-merges',
            '--date=short',
            '--format=%x1e%H%x1f%an%x1f%ae%x1f%ad%x1f%s%n%b%x1d',
            '--name-only',
            '-z',
            '--',
            pathspec,
        ],
    )
    per_path = collections.defaultdict(
        lambda: {
            'commits': 0,
            'authors': collections.Counter(),
            'reviewers': collections.Counter(),
            'last': '',
            'recent': [],
        }
    )
    commits = []
    people = {}

    def person(email, name):
        p = people.setdefault(
            email,
            {
                'name': name,
                'email': email,
                'commits': 0,
                'reviews': 0,
                'last': '',
            },
        )
        if name and not p['name']:
            p['name'] = name
        return p

    for chunk in log.split('\x1e'):
        chunk = chunk.strip()
        if not chunk:
            continue
        head, _, rest = chunk.partition('\n')
        parts = head.split('\x1f')
        if len(parts) != 5:
            continue
        h, name, email, date, subject = parts
        body, _, files = rest.partition('\x1d')
        files = [
            _join_prefix(prefix, f)
            for f in (entry.strip('\n') for entry in files.split('\0'))
            if f
        ]
        in_scope = [
            f for f in files if f.startswith(rel_root + '/') or f == rel_root
        ]
        reviewers = []
        for r_name, r_email in _REVIEWED_BY_RE.findall(body):
            if r_email == email or r_email in reviewers:
                continue  # Self-approval or duplicate trailer.
            reviewers.append(r_email)
            r = person(r_email, r_name)
            r['reviews'] += 1
            r['last'] = max(r['last'], date)
        author = person(email, name)
        author['commits'] += 1
        author['last'] = max(author['last'], date)
        commit = {
            'hash': h,
            'author': name,
            'email': email,
            'date': date,
            'subject': subject,
            'files': len(in_scope),
            'reviewers': reviewers,
        }
        commits.append(commit)
        touched = set()
        for f in in_scope:
            touched.add(f)
            d = os.path.dirname(f)
            while len(d) >= len(rel_root):
                touched.add(d)
                if d == rel_root:
                    break
                d = os.path.dirname(d)
        for p in touched:
            entry = per_path[p]
            entry['commits'] += 1
            entry['authors'][email] += 1
            for r in reviewers:
                entry['reviewers'][r] += 1
            entry['last'] = max(entry['last'], date)
            if len(entry['recent']) < _MAX_FILE_RECENT_COMMITS:
                entry['recent'].append(
                    {
                        'hash': h,
                        'subject': subject,
                        'author': email,
                        'date': date,
                    }
                )
    for entry in per_path.values():
        entry['distinct_authors'] = len(entry['authors'])
        entry['authors'] = [
            {'email': e, 'commits': c}
            for e, c in entry['authors'].most_common(_MAX_TOP_PEOPLE_PER_PATH)
        ]
        entry['reviewers'] = [
            {'email': e, 'reviews': c}
            for e, c in entry['reviewers'].most_common(_MAX_TOP_PEOPLE_PER_PATH)
        ]
    return per_path, commits[:_MAX_RECENT_COMMITS], people


_EMPTY_ACTIVITY = {
    'commits': 0,
    'last': '',
    'authors': [],
    'reviewers': [],
    'distinct_authors': 0,
    'recent': [],
}


def _activity_for(activity, rel_path):
    act = activity.get(rel_path)
    if not act:
        return dict(_EMPTY_ACTIVITY)
    return {k: act[k] for k in _EMPTY_ACTIVITY}


# ---------------------------------------------------------------------------
# Tree


def _tracked_files(repo_root, rel_root, git_root=None):
    """Git-tracked regular files under rel_root, repo-relative.

    Using the index rather than walking the filesystem keeps untracked and
    generated files out of the map. Symlinks (mode 120000) and submodules
    (160000) are dropped: a symlinked directory could otherwise loop, and
    neither has meaningful LOC or OWNERS of its own. Files deleted from the
    working tree but still tracked are skipped later, when stat fails.
    `git_root` is the submodule to list when rel_root lives in one.
    """
    git_root = git_root or repo_root
    pathspec, prefix = _git_scope(repo_root, rel_root, git_root)
    out = _run_git(git_root, ['ls-files', '--stage', '-z', '--', pathspec])
    files = []
    for entry in out.split('\0'):
        if not entry:
            continue
        meta, _, path = entry.partition('\t')
        mode = meta.split(' ', 1)[0]
        if mode in ('120000', '160000'):
            continue
        files.append(_join_prefix(prefix, path))
    return files


def _build_tree(repo_root, rel_root, owners_index, activity, files=None):
    """Builds the directory tree with per-node facts.

    `files` is the list of repo-relative paths to include (defaults to the
    git-tracked files under rel_root).
    """
    if files is None:
        files = _tracked_files(repo_root, rel_root)
    subdirs = collections.defaultdict(set)
    dir_files = collections.defaultdict(list)
    for rel_path in files:
        if any(part in _IGNORED_DIRS for part in rel_path.split('/')):
            continue
        abs_path = os.path.join(repo_root, rel_path)
        if os.path.islink(abs_path) or not os.path.isfile(abs_path):
            continue
        d = os.path.dirname(rel_path)
        dir_files[d].append(rel_path)
        while d != rel_root and len(d) > len(rel_root):
            parent = os.path.dirname(d)
            subdirs[parent].add(d)
            d = parent

    metadata_cache = {}

    def file_node(rel_path):
        name = os.path.basename(rel_path)
        rel_dir = os.path.dirname(rel_path)
        abs_path = os.path.join(repo_root, rel_path)
        resolved = owners_index.resolve_file(rel_path)
        node = {
            'name': name,
            'path': rel_path,
            'type': 'file',
            'role': _classify(name, rel_dir),
            'loc': _count_lines(abs_path),
            'description': '',
            'doc': '',
            'owners': resolved['owners'],
            'owner_rules': resolved['rules'],
            'owners_chain': resolved['chain'],
            # `per-file` rules above the nearest owners that also match.
            'inherited_owner_rules': resolved['inherited_rules'],
            # Owners to suggest for this file only when nobody else can
            # approve it.
            'last_resort': resolved['last_resort'],
            'activity': _activity_for(activity, rel_path),
        }
        if node['role'] in ('header', 'public', 'mojom'):
            node['doc'] = _header_comment(abs_path)
        return node

    def by_name(path):
        return os.path.basename(path).lower()

    def dir_node(rel_dir):
        abs_dir = os.path.join(repo_root, rel_dir)
        dirs = [dir_node(d) for d in sorted(subdirs[rel_dir], key=by_name)]
        files = [file_node(f) for f in sorted(dir_files[rel_dir], key=by_name)]
        loc = sum(c['loc'] for c in dirs + files)
        own = owners_index.parse(rel_dir)
        chain = owners_index.chain(rel_dir)
        owners, owners_source = owners_index.resolve_dir(rel_dir)
        dir_metadata, dir_metadata_source = _inherited_dir_metadata(
            repo_root, rel_dir, metadata_cache
        )
        node = {
            'name': os.path.basename(rel_dir) or rel_dir,
            'path': rel_dir,
            'type': 'dir',
            'role': 'dir',
            'loc': loc,
            'description': '',
            'file_count': len(files) + sum(d['file_count'] for d in dirs),
            'owners': owners,
            'owners_source': owners_source,
            'owners_file': own,
            'owners_chain': [c['path'] for c in chain],
            'dir_metadata': dir_metadata,
            # Differs from `<path>/DIR_METADATA` when the values are inherited.
            'dir_metadata_source': dir_metadata_source,
            'component': _component(dir_metadata),
            'readme': _readme_excerpt(abs_dir),
            'activity': _activity_for(activity, rel_dir),
            'children': dirs + files,
        }
        return node

    return dir_node(rel_root)


def _readme_excerpt(abs_dir):
    for name in ('README.md', 'README', 'readme.md'):
        readme_path = os.path.join(abs_dir, name)
        if os.path.isfile(readme_path):
            with open(readme_path, encoding='utf-8', errors='replace') as f:
                text = f.read(4000)
            paras = [
                para.strip() for para in text.split('\n\n') if para.strip()
            ]
            # Skip a leading title.
            body = [para for para in paras if not para.startswith('#')]
            return (body[0] if body else '')[:600]
    return ''


# ---------------------------------------------------------------------------
# Commands


def _walk(node):
    yield node
    for c in node.get('children', []):
        yield from _walk(c)


def extract(target, since):
    # realpath: `git rev-parse --show-toplevel` resolves symlinks, so the
    # target must too or relpath() walks out of the repo.
    target = os.path.realpath(target)
    if not os.path.isdir(target):
        raise MapError(f'{target} is not a directory')
    repo_root = _find_repo_root(target)
    git_root = _find_git_root(target)
    rel_root = os.path.relpath(target, repo_root).replace(os.sep, '/')
    if rel_root == '.':
        raise MapError('refusing to map the whole repository')

    owners_index = OwnersIndex(repo_root)
    activity, commits, people = _git_activity(
        repo_root, rel_root, since, git_root
    )
    tree = _build_tree(
        repo_root,
        rel_root,
        owners_index,
        activity,
        files=_tracked_files(repo_root, rel_root, git_root),
    )

    nodes = list(_walk(tree))
    files = [n for n in nodes if n['type'] == 'file']
    return {
        'schema_version': _SCHEMA_VERSION,
        'root': rel_root,
        # Used only to compute the default output path; `render` strips it so
        # the shareable HTML doesn't carry a local home directory.
        'repo_root': repo_root,
        # Local time with its UTC offset.
        'generated_at': datetime.datetime.now()
        .astimezone()
        .isoformat(timespec='seconds'),
        'since': since,
        'summary': '',
        'highlights': [],
        'stats': {
            'files': len(files),
            'dirs': len(nodes) - len(files),
            'loc': tree['loc'],
            'commits': tree['activity']['commits'],
            'contributors': sum(1 for p in people.values() if p['commits']),
            'reviewers': sum(1 for p in people.values() if p['reviews']),
        },
        # Sorted by total involvement; the UI re-sorts per view.
        'people': sorted(
            people.values(), key=lambda p: -(p['commits'] + p['reviews'])
        ),
        # Every OWNERS file consulted, so the UI can tell "owner via a parent
        # OWNERS" apart from "not an owner at all" without repeating the
        # (often long) parent lists on each node.
        'owners_files': {
            info['path']: info['owners'] for info in owners_index.all_parsed()
        },
        'recent_commits': commits,
        'tree': tree,
    }


def _embed_json(data):
    """Serializes `data` for an inline <script> block.

    Every `<` becomes `\\u003c`, which is the same string to JSON/JS but means
    the HTML tokenizer never sees `</script`, `<!--` or `<script` inside the
    block (the last two together would switch it into the "double escaped"
    state and swallow the real closing tag).
    """
    return json.dumps(data, ensure_ascii=False).replace('<', '\\u003c')


def render(data, template_path=_TEMPLATE):
    version = data.get('schema_version')
    if version != _SCHEMA_VERSION:
        raise MapError(
            f'map JSON has schema_version {version!r}, this script '
            f'expects {_SCHEMA_VERSION}; re-run `extract`'
        )
    with open(template_path, encoding='utf-8') as f:
        template = f.read()
    public = {k: v for k, v in data.items() if k != 'repo_root'}
    marker = '/*__CODEBASE_MAP_DATA__*/'
    if marker not in template:
        raise MapError(f'template {template_path} is missing {marker}')
    return template.replace(marker, _embed_json(public), 1)


# ---------------------------------------------------------------------------
# Annotation (the agent step)


def targets(data, hot=_DEFAULT_HOT_FILES):
    """Lists the nodes an agent should write a description for.

    Every directory, plus the `hot` most-changed files (by distinct commits
    in the activity window; ties broken by LOC). Each target carries the
    context an agent needs to write one good sentence without re-reading the
    whole file: existing doc comment, README excerpt, recent commit subjects,
    and child names for directories.
    """
    nodes = list(_walk(data['tree']))
    dirs = [n for n in nodes if n['type'] == 'dir']
    files = [n for n in nodes if n['type'] == 'file']
    hot_files = sorted(
        files, key=lambda n: (-n['activity']['commits'], -n['loc'])
    )
    hot_files = [f for f in hot_files[:hot] if f['activity']['commits'] > 0]

    out = []
    for n in dirs + hot_files:
        t = {
            'path': n['path'],
            'type': n['type'],
            'current_description': n.get('description', ''),
            'commits': n['activity']['commits'],
            'recent_subjects': [c['subject'] for c in n['activity']['recent']],
        }
        if n['type'] == 'dir':
            t['readme'] = n.get('readme', '')
            t['children'] = [
                c['name'] + ('/' if c['type'] == 'dir' else '')
                for c in n['children']
            ]
            dm = n.get('dir_metadata') or {}
            if dm:
                t['dir_metadata'] = dm
        else:
            t['role'] = n['role']
            t['doc'] = n.get('doc', '')
            t['loc'] = n['loc']
        out.append(t)
    return {
        'root': data['root'],
        'summary': data.get('summary', ''),
        'highlights': data.get('highlights', []),
        'targets': out,
    }


def annotate(data, annotations):
    """Merges agent-written text into the map.

    `annotations` shape:
      {
        "summary": "...",             # 2-3 sentences for the header
        "highlights": ["...", ...],   # 3-5 bullets
        "descriptions": {"<path>": "one sentence", ...}
      }
    Paths are repo-relative (as in `targets` output). Unknown paths and
    entries of the wrong type are reported in `warnings` and skipped, so one
    bad entry doesn't lose the whole batch.

    Returns {'applied': int, 'unknown': [path, ...], 'warnings': [str, ...]}.
    """
    if not isinstance(annotations, dict):
        return {
            'applied': 0,
            'unknown': [],
            'warnings': ['annotations are not a JSON object; ignored'],
        }
    warnings = []
    summary = annotations.get('summary')
    if isinstance(summary, str):
        if summary.strip():
            data['summary'] = summary.strip()
    elif summary is not None:
        warnings.append('summary is not a string; ignored')

    highlights = annotations.get('highlights')
    if isinstance(highlights, str):
        # A lone string would otherwise become one bullet per character.
        highlights = [highlights]
    if isinstance(highlights, list):
        good = [
            h.strip() for h in highlights if isinstance(h, str) and h.strip()
        ]
        if len(good) != len(highlights):
            warnings.append('skipped non-string or empty highlights')
        if good:
            data['highlights'] = good
    elif highlights is not None:
        warnings.append('highlights is not a list; ignored')

    descriptions = annotations.get('descriptions') or {}
    if not isinstance(descriptions, dict):
        warnings.append('descriptions is not an object; ignored')
        descriptions = {}
    by_path = {n['path']: n for n in _walk(data['tree'])}
    unknown = []
    applied = 0
    for path, text in descriptions.items():
        node = by_path.get(path)
        if node is None:
            unknown.append(path)
            continue
        if not isinstance(text, str):
            warnings.append(f'description for {path} is not a string; skipped')
            continue
        if text.strip():
            node['description'] = text.strip()
            applied += 1
    return {'applied': applied, 'unknown': unknown, 'warnings': warnings}


def map_slug(rel):
    """File name stem for a repo-relative directory: `a/b_c` -> `a__b_c`.

    `/` becomes `__` rather than `_` so `a/b_c` and `a_b/c` don't collide.
    """
    return rel.strip('/').replace('/', '__') or 'root'


def default_output(data_or_dir, repo_root=None):
    """`<repo>/out/codebase_map/<map_slug(rel_dir)>.html`."""
    if isinstance(data_or_dir, dict):
        repo_root = data_or_dir['repo_root']
        rel = data_or_dir['root']
    else:
        target = os.path.realpath(data_or_dir)
        repo_root = repo_root or _find_repo_root(target)
        rel = os.path.relpath(target, repo_root).replace(os.sep, '/')
    return os.path.join(repo_root, _DEFAULT_OUT_DIR, map_slug(rel) + '.html')


# ---------------------------------------------------------------------------
# Serving


class _MapHandler(http.server.BaseHTTPRequestHandler):
    """Serves `*.html` maps from one directory, and nothing else.

    Only top-level `.html` files are reachable, so serving `/tmp/map.html`
    doesn't publish the rest of `/tmp`. Requests whose `Host` isn't in
    `allowed_hosts` (loopback names, plus this machine's own names in proxy
    mode) are refused, which blocks DNS-rebinding pages from reading maps
    through the user's browser: such a page always arrives under the
    attacker's domain. `_PROBE_PATH` reports which directory is served (as
    a hash, so the path isn't disclosed), and in which mode, so a second
    `serve` can reuse this server.
    """

    directory = None  # Set on a per-server subclass.
    allowed_hosts = frozenset(_LOCAL_HOSTS)
    allowed_suffixes = ()
    proxy = False

    def log_message(self, *args):
        pass

    def _host_name(self):
        host = self.headers.get('Host', '')
        # Strip the port; IPv6 literals arrive as `[::1]:8765`.
        name = host.rsplit(':', 1)[0] if not host.endswith(']') else host
        return name.strip('[]').lower()

    def _host_allowed(self, host):
        if host in self.allowed_hosts:
            return True
        return any(
            host.endswith(s) and len(host) > len(s)
            for s in self.allowed_suffixes
        )

    def _send(self, code, body, content_type='text/plain; charset=utf-8'):
        data = body.encode('utf-8') if isinstance(body, str) else body
        self.send_response(code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(data)

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        host = self._host_name()
        if not self._host_allowed(host):
            if self.proxy and host:
                # Proxies differ in the Host they forward; say what arrived
                # so the user can pass it to --allow-host. (Empty Hosts are
                # usually health probes; not worth a line each.)
                print(
                    f'refused request with Host {host!r}; if that is your '
                    'proxy, re-run with --allow-host ' + host,
                    file=sys.stderr,
                )
            self._send(403, 'forbidden: unexpected Host header\n')
            return
        # Only accepted requests count as activity for the idle timeout.
        self.server.last_request = time.monotonic()
        path = urllib.parse.unquote(urllib.parse.urlsplit(self.path).path)
        if path == _PROBE_PATH:
            probe = {
                'directory_id': _directory_id(self.directory),
                'proxy': self.proxy,
            }
            self._send(200, json.dumps(probe), 'application/json')
            return
        name = path.lstrip('/')
        if (
            not name
            or '/' in name
            or '\\' in name
            or not name.endswith('.html')
        ):
            self._send(404, 'not found\n')
            return
        file_path = os.path.join(self.directory, name)
        if os.path.islink(file_path) or not os.path.isfile(file_path):
            self._send(404, 'not found\n')
            return
        with open(file_path, 'rb') as f:
            self._send(200, f.read(), 'text/html; charset=utf-8')


def _directory_id(directory):
    """Identifies a served directory in the probe without naming it."""
    return hashlib.sha256(directory.encode('utf-8')).hexdigest()


def _running_server_for(directory, port, proxy=False):
    """True if a codebase-map server on `port` serves `directory` in the
    same mode (loopback-only or `proxy`).

    Talks to 127.0.0.1 directly (both modes accept it) rather than via
    urllib, which would honor `http_proxy` and resolve `localhost` itself.
    """
    conn = http.client.HTTPConnection('127.0.0.1', port, timeout=1)
    try:
        conn.request('GET', _PROBE_PATH, headers={'Host': f'localhost:{port}'})
        resp = conn.getresponse()
        if resp.status != 200:
            return False
        info = json.loads(resp.read())
    except (OSError, ValueError, http.client.HTTPException):
        return False
    finally:
        conn.close()
    return (
        isinstance(info, dict)
        and info.get('directory_id') == _directory_id(directory)
        and bool(info.get('proxy')) == proxy
    )


def _machine_names():
    """This machine's own host names, as a proxy would address it."""
    names = {socket.gethostname(), socket.getfqdn(), platform.node()}
    return {n.lower() for n in names if n and n != 'localhost'}


class _DualStackServer(http.server.ThreadingHTTPServer):
    """Listens on every IPv4 and IPv6 interface (proxy mode)."""

    address_family = socket.AF_INET6

    def server_bind(self):
        with contextlib.suppress(OSError, AttributeError):
            self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        super().server_bind()


def _make_server(port, handler, proxy):
    if not proxy:
        return http.server.ThreadingHTTPServer(('127.0.0.1', port), handler)
    if socket.has_ipv6:
        try:
            return _DualStackServer(('::', port), handler)
        except OSError:
            pass  # No IPv6 on this host; fall through to IPv4.
    return http.server.ThreadingHTTPServer(('0.0.0.0', port), handler)


def _is_remote_session():
    """Best-effort guess at whether a browser opened here would be invisible.

    True on cloudtop / SSH sessions, where `xdg-open` launches a browser on the
    remote host that the user can't see.
    """
    if any(
        os.environ.get(v) for v in ('SSH_CONNECTION', 'SSH_CLIENT', 'SSH_TTY')
    ):
        return True
    if sys.platform.startswith('linux') and not (
        os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')
    ):
        return True
    return False


def serve(
    html_path,
    port=None,
    open_browser=True,
    idle_timeout_min=_DEFAULT_IDLE_TIMEOUT_MIN,
    on_ready=None,
    proxy=False,
    allow_hosts=(),
    stop_event=None,
):
    """Serves the maps in `html_path`'s directory and blocks.

    Tries `_PREFERRED_PORT` first so re-runs reuse an already-forwarded port,
    then falls back to an ephemeral one. If a codebase-map server is already
    serving the same directory, in the same mode, on the preferred port,
    prints its URL and returns 'reused' instead of starting a second one.
    Otherwise returns 'stopped' after `idle_timeout_min` minutes without a
    request (0 disables; fractions are allowed) so background servers don't
    accumulate. `on_ready(port)` is called once the server is listening, and
    setting `stop_event` (a `threading.Event`) stops it from another thread.

    By default only 127.0.0.1 is bound, so the map is reachable from this
    machine or through `ssh -L` / IDE port forwarding. `proxy=True` binds
    every interface and also accepts this machine's own host names (plus
    `allow_hosts`), for browser-side proxies such as
    `http://<cloudtop>.c.googlers.com:<port>/` that connect over the network.
    Anyone who can reach the port can then read the maps, so it is opt-in.

    Prints the URL to stderr: remote IDEs (VS Code Remote, Cloud Workstations,
    etc.) pick up `localhost:<port>` URLs from the terminal and offer to
    forward them.
    """
    html_path = os.path.abspath(html_path)
    if not os.path.isfile(html_path):
        raise MapError(f'{html_path} does not exist')
    directory = os.path.dirname(html_path)
    name = os.path.basename(html_path)
    quoted = urllib.parse.quote(name)
    host_for_url = max(_machine_names(), key=len, default='') if proxy else ''

    def urls(p):
        out = [f'http://localhost:{p}/{quoted}']
        if proxy and host_for_url:
            out.insert(0, f'http://{host_for_url}:{p}/{quoted}')
        return out

    reuse_port = port or _PREFERRED_PORT
    if _running_server_for(directory, reuse_port, proxy):
        listing = '\n'.join(f'  {u}' for u in urls(reuse_port))
        print(
            f'\nAlready serving this directory at:\n\n{listing}\n',
            file=sys.stderr,
        )
        return 'reused'

    allowed = set(_LOCAL_HOSTS)
    suffixes = ()
    if proxy:
        allowed |= _machine_names()
        # `--allow-host .example.com` accepts every name under example.com.
        extra = {h.lower() for h in allow_hosts}
        allowed |= {h for h in extra if not h.startswith('.')}
        suffixes = tuple(sorted(h for h in extra if h.startswith('.')))
    handler = type(
        '_BoundMapHandler',
        (_MapHandler,),
        {
            'directory': directory,
            'allowed_hosts': frozenset(allowed),
            'allowed_suffixes': suffixes,
            'proxy': proxy,
        },
    )
    candidates = [port] if port else [_PREFERRED_PORT, 0]
    httpd = None
    for candidate in candidates:
        try:
            httpd = _make_server(candidate, handler, proxy)
            break
        except OSError:
            continue
    if httpd is None:
        raise MapError(f'could not bind a port (tried {candidates})')
    httpd.daemon_threads = True
    httpd.last_request = time.monotonic()

    port = httpd.server_address[1]
    url = urls(port)[-1]
    remote = _is_remote_session()

    listing = '\n'.join(f'  {u}' for u in urls(port))
    print(f'\nServing codebase map at:\n\n{listing}\n', file=sys.stderr)
    if proxy:
        print(
            'Proxy mode: listening on all interfaces; anyone who can reach '
            f'port {port} can read the maps in {directory}.',
            file=sys.stderr,
        )
    elif remote:
        print(
            'This looks like a remote session (cloudtop/SSH), so the\n'
            'browser has to run on your local machine. Either:\n'
            f'  * click the URL above / accept your IDE\'s "forward port '
            f'{port}"\n'
            '    prompt (VS Code: Ports panel -> Forward a Port), or\n'
            f'  * from your local machine: ssh -L {port}:localhost:{port} '
            f'{platform.node()}\n'
            '  * or, if you open cloudtop ports through a proxy URL, re-run\n'
            '    with --proxy.\n',
            file=sys.stderr,
        )
    elif open_browser:
        webbrowser.open(url)
    if idle_timeout_min:
        print(
            f'Stops after {idle_timeout_min} min without requests, or on '
            'Ctrl-C.',
            file=sys.stderr,
        )
    else:
        print('Press Ctrl-C to stop.', file=sys.stderr)

    thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    thread.start()
    if on_ready:
        on_ready(port)
    poll = min(10, idle_timeout_min * 60) if idle_timeout_min else 10
    stop = stop_event or threading.Event()
    try:
        while thread.is_alive() and not stop.wait(poll):
            idle = time.monotonic() - httpd.last_request
            if idle_timeout_min and idle > idle_timeout_min * 60:
                print('Idle timeout reached; stopping.', file=sys.stderr)
                break
    except KeyboardInterrupt:
        pass
    finally:
        httpd.shutdown()
        httpd.server_close()
    return 'stopped'


def _write(out, path, label='wrote'):
    if path == '-':
        sys.stdout.write(out)
        return
    os.makedirs(os.path.dirname(os.path.abspath(path)) or '.', exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(out)
    print(f'{label} {path}', file=sys.stderr)


def _load_json(path):
    with open(path, encoding='utf-8') as f:
        return json.load(f)


def _add_idle_timeout_arg(parser):
    parser.add_argument(
        '--idle-timeout',
        type=float,
        default=_DEFAULT_IDLE_TIMEOUT_MIN,
        metavar='MIN',
        help='stop serving after MIN minutes without a request '
        f'(default {_DEFAULT_IDLE_TIMEOUT_MIN}; 0 = never)',
    )


def _add_serve_args(parser):
    parser.add_argument(
        '--no-open',
        action='store_true',
        help="don't try to launch a browser even when local",
    )
    parser.add_argument(
        '--proxy',
        action='store_true',
        help='listen on all interfaces and accept this machine\'s host name, '
        'for browser proxies like http://<cloudtop>.c.googlers.com:<port>/ '
        '(anyone who can reach the port can read the maps)',
    )
    parser.add_argument(
        '--allow-host',
        action='append',
        default=[],
        metavar='NAME',
        help='with --proxy, also accept requests for this Host; a leading '
        'dot (.example.com) accepts every name under it (repeatable)',
    )


def _serve_from_args(html, args):
    return serve(
        html,
        args.port,
        open_browser=not args.no_open,
        idle_timeout_min=args.idle_timeout,
        proxy=args.proxy,
        allow_hosts=args.allow_host,
    )


def main(argv):
    try:
        _run(argv)
    except MapError as e:
        sys.exit(f'error: {e}')


def _run(argv):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = parser.add_subparsers(dest='cmd', required=True)

    p = sub.add_parser('extract', help='write map JSON for a directory')
    p.add_argument('dir')
    p.add_argument('--since', default='6.months')
    p.add_argument('-o', '--output', default='-')

    p = sub.add_parser(
        'targets', help='list the nodes an agent should describe'
    )
    p.add_argument('json')
    p.add_argument(
        '--hot',
        type=int,
        default=_DEFAULT_HOT_FILES,
        help='number of most-changed files to include '
        f'(default {_DEFAULT_HOT_FILES})',
    )
    p.add_argument('-o', '--output', default='-')

    p = sub.add_parser(
        'annotate', help='merge agent-written descriptions into map JSON'
    )
    p.add_argument('json')
    p.add_argument('annotations')
    p.add_argument(
        '-o',
        '--output',
        default=None,
        help='defaults to overwriting the input JSON',
    )

    p = sub.add_parser('render', help='render map JSON to HTML')
    p.add_argument('json')
    p.add_argument('--template', default=_TEMPLATE)
    p.add_argument(
        '-o',
        '--output',
        default=None,
        help='defaults to out/codebase_map/<dir>.html',
    )
    p.add_argument('--serve', action='store_true')
    p.add_argument('--port', type=int, default=None)
    _add_idle_timeout_arg(p)
    _add_serve_args(p)

    p = sub.add_parser('build', help='extract and render in one step')
    p.add_argument('dir')
    p.add_argument('--since', default='6.months')
    p.add_argument('--template', default=_TEMPLATE)
    p.add_argument(
        '-o',
        '--output',
        default=None,
        help='defaults to out/codebase_map/<dir>.html',
    )
    p.add_argument(
        '--serve',
        action='store_true',
        help='after writing, serve the map over localhost '
        '(works from cloudtop/SSH via port forwarding)',
    )
    p.add_argument('--port', type=int, default=None)
    _add_idle_timeout_arg(p)
    _add_serve_args(p)

    p = sub.add_parser('serve', help='serve an existing map HTML on localhost')
    p.add_argument('html')
    p.add_argument(
        '--port',
        type=int,
        default=None,
        help=f'port to bind (default: {_PREFERRED_PORT}, '
        'falling back to a free port)',
    )
    _add_serve_args(p)
    _add_idle_timeout_arg(p)

    args = parser.parse_args(argv)

    if args.cmd == 'serve':
        _serve_from_args(args.html, args)
        return

    if args.cmd == 'extract':
        _write(json.dumps(extract(args.dir, args.since), indent=1), args.output)
        return

    if args.cmd == 'targets':
        _write(
            json.dumps(targets(_load_json(args.json), args.hot), indent=1),
            args.output,
        )
        return

    if args.cmd == 'annotate':
        data = _load_json(args.json)
        result = annotate(data, _load_json(args.annotations))
        for path in result['unknown']:
            print(f'warning: no such node in map: {path}', file=sys.stderr)
        for warning in result['warnings']:
            print(f'warning: {warning}', file=sys.stderr)
        _write(
            json.dumps(data, indent=1),
            args.output or args.json,
            label=f'annotated {result["applied"]} node(s), wrote',
        )
        return

    # render / build
    if args.cmd == 'render':
        data = _load_json(args.json)
    else:
        data = extract(args.dir, args.since)
    output = args.output or default_output(data)
    if output == '-' and args.serve:
        raise MapError('--serve requires a file output')
    _write(render(data, args.template), output)
    if args.serve:
        _serve_from_args(output, args)
    elif output != '-':
        print(
            f'to view: {sys.argv[0]} serve {output} --no-open', file=sys.stderr
        )


if __name__ == '__main__':
    main(sys.argv[1:])
