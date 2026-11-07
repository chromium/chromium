# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Suggests code owners/maintainers for modified files (Code Health submission).

Groups modified files by directory, resolves the immediate non-broad `OWNERS`
(following `file://`/`include` and matching `per-file` rules; broad parent
`OWNERS` are only a fallback) and ranks people by recent git commits and
`Reviewed-by` trailers. The current git user, bots and groups are never
suggested. Always exits with 0.

Usage: find_owners.py [--files <file>...]
"""

import argparse
import collections
import fnmatch
import os
import posixpath
import re
import subprocess
import sys

# Emails sharing a local part across these domains are the same person.
_MERGED_DOMAINS = ('chromium.org', 'google.com')
_BOT_PATTERNS = (
    '*-review@*', '*-reviews@*', '*+reviews@*', 'reviews@*',
    '*-owners@*', '*+owners@*', 'owners@*', '*-team@*', 'mdb.*',
    '*gserviceaccount.com', '*noreply*', 'tricorder*', 'autoroller*',
    'chromium-autoroll*', 'chrome-metrics-team*', 'clank-*-team*',
)  # yapf: disable
# OWNERS files in these directories (or at depth <= 1) are too broad to name a
# specific owner and are only used as a fallback.
_BROAD_OWNERS_DIRS = frozenset({
    'chrome/android', 'chrome/browser', 'chrome/common', 'chrome/renderer',
    'content/browser', 'content/renderer', 'components/embedder_support',
})  # yapf: disable
_BROAD_OWNERS_MAX_DEPTH = 1
_LAST_RESORT = '{LAST_RESORT_SUGGESTION}'

_COUNTS = ('file_commits', 'file_reviews', 'dir_commits', 'dir_reviews')
_EMAIL_RE = re.compile(r'[\w.%+\-]+@[A-Za-z0-9\-]+(?:\.[A-Za-z0-9\-]+)+')
_INCLUDE_RE = re.compile(r'^(?:include\s+|file:\s*)(\S+)$')
_PER_FILE_RE = re.compile(r'^per-file\s+(.+?)\s*=\s*(.*)$')
_GIT_LOG_FORMAT = (
    '--format=%x1e%ae%x1f%(trailers:key=Reviewed-by,valueonly,separator=%x1f)'
)


def _git(repo_root, *args):
    """Returns stdout of `git |args|` in |repo_root|, or '' on any failure."""
    try:
        result = subprocess.run(
            ['git', '-c', 'core.quotePath=false', *args],
            cwd=repo_root,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            errors='replace',
            check=False,
        )
    except OSError:
        return ''
    return result.stdout if result.returncode == 0 else ''


def _identity_key(email):
    """Returns the merge key: local part for Google domains, else the email."""
    local, _, domain = email.strip().lower().partition('@')
    return local if domain in _MERGED_DOMAINS else email.strip().lower()


def _is_bot(email):
    email = email.strip().lower()
    return not email or any(
        fnmatch.fnmatchcase(email, p) for p in _BOT_PATTERNS
    )


def _repo_relative(path, repo_root, cwd):
    """Returns the repo-relative path of |path|, or '' if outside the repo.

    Relative paths resolve against |cwd| when it is inside the repo, otherwise
    against |repo_root|. Directories are realpath'd on both sides so symlinked
    workspaces work.
    """
    root = os.path.realpath(repo_root) + os.sep
    if not os.path.isabs(path):
        in_repo = (os.path.realpath(cwd) + os.sep).startswith(root)
        path = os.path.join(cwd if in_repo else root, path)
    head, tail = os.path.split(os.path.normpath(path))
    full = os.path.join(os.path.realpath(head), tail)
    return full[len(root) :] if full.startswith(root) else ''


def detect_modified_files(repo_root):
    """Returns sorted tracked files that differ from HEAD (never untracked).

    If the working tree is clean and the branch is not main, returns the files
    changed since the merge-base with main instead.
    """
    paths = _git(repo_root, 'diff', '--name-only', 'HEAD').split('\n')
    branch = _git(repo_root, 'rev-parse', '--abbrev-ref', 'HEAD').strip()
    if not any(paths) and branch not in ('', 'main'):
        bases = (
            _git(repo_root, 'merge-base', 'HEAD', ref).strip()
            for ref in ('origin/main', 'main')
        )
        base = next((b for b in bases if b), 'HEAD~1')
        paths = _git(repo_root, 'diff', '--name-only', base, 'HEAD').split('\n')
    return sorted({p.strip() for p in paths if p.strip()})


def is_broad_owners_path(owners_rel):
    """Returns whether the repo-relative OWNERS file is a broad fallback."""
    directory = posixpath.dirname(owners_rel)
    depth = len(directory.split('/')) if directory else 0
    return depth <= _BROAD_OWNERS_MAX_DEPTH or directory in _BROAD_OWNERS_DIRS


def _read_text(repo_root, rel):
    """Returns the text of repo-relative |rel|, or None if missing/outside."""
    root = os.path.realpath(repo_root)
    path = os.path.realpath(os.path.join(root, rel))
    if not path.startswith(root + os.sep) or not os.path.isfile(path):
        return None
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            return f.read()
    except OSError:
        return None


def _include_target(line, base_dir):
    """Returns the repo-relative target of an include/file: line, or None."""
    match = _INCLUDE_RE.match(line)
    if not match:
        return None
    spec = match.group(1)
    if spec.startswith('/'):  # Both `/x` and `//x` are repo-root relative.
        spec = spec.lstrip('/')
    else:
        spec = posixpath.join(base_dir, spec)
    target = posixpath.normpath(spec)
    return None if target == '..' or target.startswith('../') else target


def _read_owners(repo_root, owners_rel, filename, visiting=()):
    """Returns (specific, fallback, noparent) of an OWNERS file for |filename|.

    |specific| and |fallback| are lists of (email, source OWNERS path). Returns
    None if the file is missing or is already being resolved (include cycle).
    Broad OWNERS files, also when reached through includes, only add fallback
    owners.
    """
    text = _read_text(repo_root, owners_rel)
    if text is None or owners_rel in visiting:
        return None
    visiting += (owners_rel,)  # Per-path stack so sibling includes can repeat.
    broad = is_broad_owners_path(owners_rel)
    specific, fallback, noparent, in_section = [], [], False, False
    for raw in text.splitlines():
        body, _, comment = raw.partition('#')
        line = body.strip()
        marker = _LAST_RESORT in comment
        if not line:
            in_section = in_section or marker  # Standalone: rest of the file.
            continue
        owners = fallback if broad or marker or in_section else specific
        per_file = _PER_FILE_RE.match(line)
        if per_file:
            globs = [g.strip() for g in per_file.group(1).split(',')]
            if not any(fnmatch.fnmatchcase(filename, g) for g in globs):
                continue
            line = per_file.group(2).strip()
        target = _include_target(line, posixpath.dirname(owners_rel))
        if line == 'set noparent':
            noparent = True
        elif target:
            sub = _read_owners(repo_root, target, filename, visiting)
            owners.extend(sub[0] if sub else [])
            fallback.extend(sub[1] if sub else [])
        else:
            emails = [t.strip() for t in line.lower().split(',')]
            # `*` wildcards do not match and are therefore ignored.
            owners.extend(
                (e, owners_rel) for e in emails if _EMAIL_RE.fullmatch(e)
            )
    return specific, fallback, noparent


def resolve_owners(repo_root, rel_path):
    """Returns (specific, fallback, unmatched) for |rel_path|.

    |specific| and |fallback| are (email, source) lists; |unmatched| is set if
    the immediate non-broad OWNERS exists but yields no owners for the file.
    Works for deleted files because only the path is used.
    """
    specific, fallback, unmatched = [], [], False
    filename = posixpath.basename(rel_path)
    directory = posixpath.dirname(rel_path)
    while True:
        owners_rel = posixpath.join(directory, 'OWNERS')
        parsed = _read_owners(repo_root, owners_rel, filename)
        if parsed:
            found, more_fallback, noparent = parsed
            if found and not specific:
                specific = found
            elif found:
                # Upper primary owners stay as fallback behind the closer ones.
                fallback += found
            elif directory == posixpath.dirname(rel_path):
                unmatched = not is_broad_owners_path(owners_rel)
            fallback += more_fallback
            if noparent:
                break
        if not directory:
            break
        directory = posixpath.dirname(directory)
    return specific, fallback, unmatched


def _activity(repo_root, paths, max_commits, keep, ident):
    """Returns (authored, reviewed) Counters of identities touching |paths|.

    Looks at the last year first and falls back to the full history if empty.
    """
    authored, reviewed = collections.Counter(), collections.Counter()
    for since in (['--since=1.year'], []):
        output = _git(
            repo_root,
            'log',
            '--no-merges',
            '-n',
            str(max_commits),
            *since,
            _GIT_LOG_FORMAT,
            '--',
            *paths,
        )
        records = output.split('\x1e')[1:]
        for author, *trailers in (r.split('\x1f') for r in records):
            author_key = ident(author) if keep(author) else None
            if author_key:
                authored[author_key] += 1
            reviewers = _EMAIL_RE.findall(' '.join(trailers))
            for key in {ident(r) for r in reviewers if keep(r)} - {author_key}:
                reviewed[key] += 1
        if records:
            break
    return authored, reviewed


def _rank(candidate):
    return (
        not candidate['specific'],
        -(candidate['file_commits'] + candidate['file_reviews']),
        -(candidate['dir_commits'] + candidate['dir_reviews']),
        not candidate['fallback'],
        candidate['key'],
    )


def _owner_keys(pairs, keep, ident):
    """Returns ([identity keys], [sources]) of kept (email, source) pairs."""
    kept = [(ident(email), source) for email, source in pairs if keep(email)]
    return (
        list(dict.fromkeys(k for k, _ in kept)),
        list(dict.fromkeys(s for _, s in kept)),
    )


def _analyze_directory(
    repo_root, directory, files, max_commits, top, keep, ident
):
    """Returns the suggestions of one directory (identity keys, not emails)."""
    resolutions = [resolve_owners(repo_root, f) for f in files]
    specific, specific_sources = _owner_keys(
        [p for r in resolutions for p in r[0]], keep, ident
    )
    fallback, fallback_sources = _owner_keys(
        [p for r in resolutions for p in r[1]], keep, ident
    )
    unmatched = [
        posixpath.basename(f) for f, r in zip(files, resolutions) if r[2]
    ]
    note = ''
    if unmatched:  # One line per directory, not per file.
        note = (
            f'{posixpath.join(directory, "OWNERS")} exists but has no '
            f'owners matching {", ".join(unmatched)} (e.g. only unrelated '
            'per-file rules).'
        )
    file_authored, file_reviewed = _activity(
        repo_root, files, max_commits, keep, ident
    )
    # A root-level file has no directory, and `git log -- .` would scan the
    # whole repo.
    dir_authored, dir_reviewed = (
        _activity(repo_root, [directory], max_commits, keep, ident)
        if directory
        else (collections.Counter(), collections.Counter())
    )

    def candidate(key):
        return {'key': key, 'specific': key in specific,
                'fallback': key in fallback,
                'file_commits': file_authored[key],
                'file_reviews': file_reviewed[key],
                'dir_commits': dir_authored[key],
                'dir_reviews': dir_reviewed[key]}  # yapf: disable

    keys = set(specific) | set(file_authored) | set(file_reviewed)
    keys |= set(dir_authored) | set(dir_reviewed)
    recommended = sorted(map(candidate, keys), key=_rank)[:top]
    return {
        'directory': directory,
        'files': files,
        'specific_owners': specific,
        'specific_sources': specific_sources,
        'fallback_owners': fallback,
        'fallback_sources': fallback_sources,
        'note': note,
        'recommended': recommended or [candidate(k) for k in fallback[:top]],
    }


def _overall(directories, top):
    """Merges per-directory recommendations; each directory counts once."""
    totals = {}
    for info in directories:
        for c in info['recommended']:
            t = totals.setdefault(
                c['key'], dict(c, directories=0, **dict.fromkeys(_COUNTS, 0))
            )
            t['directories'] += 1
            t['specific'] = t['specific'] or c['specific']
            t['fallback'] = t['fallback'] or c['fallback']
            for field in _COUNTS:
                t[field] += c[field]
    return sorted(totals.values(), key=lambda t: (-t['directories'], _rank(t)))[
        :top
    ]


def analyze(repo_root, files, max_commits=100, top=5):
    """Returns the structured owner suggestions for repo-relative |files|."""
    user_key = _identity_key(_git(repo_root, 'config', 'user.email'))
    names = {}  # Identity key -> display email, preferring chromium.org.

    def ident(email):
        email = email.strip().lower()
        key = _identity_key(email)
        if key not in names or email.endswith('@chromium.org'):
            names[key] = email
        return key

    def keep(email):
        return not _is_bot(email) and _identity_key(email) != user_key

    by_dir = {}
    for f in files:
        by_dir.setdefault(posixpath.dirname(f), []).append(f)
    directories = [
        _analyze_directory(
            repo_root, d, by_dir[d], max_commits, top, keep, ident
        )
        for d in sorted(by_dir)
    ]
    overall = _overall(directories, top)

    def label(candidates):
        return [dict(c, email=names[c['key']]) for c in candidates]

    for info in directories:
        for field in ('specific_owners', 'fallback_owners'):
            info[field] = [names[k] for k in info[field]]
        info['recommended'] = label(info['recommended'])
    return {
        'files': list(files),
        'directories': directories,
        'overall': label(overall),
    }


def _format_candidate(c):
    tag = (
        ' [OWNERS]'
        if c['specific']
        else ' [fallback OWNERS]'
        if c['fallback']
        else ''
    )
    return (
        f'{c["email"]}  file {c["file_commits"]}c/{c["file_reviews"]}r, '
        f'dir {c["dir_commits"]}c/{c["dir_reviews"]}r{tag}'
    )


def _report(result):
    if not result['files']:
        print('No modified files found; no owners to suggest.')
        return
    print(f'Suggested owners for {len(result["files"])} modified file(s):')
    for info in result['directories']:
        names = ', '.join(posixpath.basename(f) for f in info['files'])
        print(f'\n{info["directory"] or "."}/ ({names})')
        if info['specific_owners']:
            print(
                f'  Resolved OWNERS: {", ".join(info["specific_sources"])} '
                f'-> {", ".join(info["specific_owners"])}'
            )
        else:
            print('  Resolved OWNERS: none.')
        if info['note']:
            print(f'  Note: {info["note"]}')
        print(
            '  Recommended:' if info['recommended'] else '  Recommended: none.'
        )
        for c in info['recommended']:
            print(f'    {_format_candidate(c)}')
        if info['fallback_owners']:  # Listed under Recommended if no one else.
            print(
                f'  Fallback OWNERS: {", ".join(info["fallback_sources"])} '
                f'({len(info["fallback_owners"])} owner(s))'
            )
    if len(result['directories']) > 1 and result['overall']:
        print('\nOverall:')
        for c in result['overall']:
            print(f'  {_format_candidate(c)}  ({c["directories"]} dir(s))')


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        '--files',
        nargs='*',
        help='Modified files (relative to the cwd or repo root, or absolute). '
        'Defaults to tracked files that differ from HEAD, or the files changed '
        'on this branch since main when the working tree is clean.',
    )
    args = parser.parse_args(argv)
    cwd = os.getcwd()
    repo_root = _git(cwd, 'rev-parse', '--show-toplevel').strip() or cwd
    if args.files:
        rel_paths = (_repo_relative(f, repo_root, cwd) for f in args.files)
        files = sorted({f for f in rel_paths if f})
    else:
        files = detect_modified_files(repo_root)
    _report(analyze(repo_root, files))
    return 0


if __name__ == '__main__':
    sys.exit(main())
