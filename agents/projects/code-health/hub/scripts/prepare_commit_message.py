# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates and validates Code Health commit messages.

Pass the description on stdin (`--description -`, or just pipe it) so quotes
and backticks never go through the shell. Exits 0 on success, 1 if validation
fails, and 2 on bad arguments or unreadable input.

The hashtags and parent bug are fixed per skill, so pass `--skill`.
Never include `LSC Doc:` in a commit message.

Usage:
  prepare_commit_message.py --skill <name> --title <title> \\
      [--bug <id>]... [--footer <line>]... \\
      [--description <text|->] [--out-commit <path>]
  prepare_commit_message.py --skill <name> --validate <path> [--bug <id>]...
"""

import argparse
import re
import sys
import textwrap
from typing import NamedTuple

MAX_LINE_LENGTH = 72
BANNED_TAGS = ('CONV=', 'TAG=', 'Test:', 'Tested:')
# Stale generator attribution that validation rejects.
GENERATOR_DISCLAIMER = 'This change was generated using'


class SkillConfig(NamedTuple):
    """Commit message settings that are fixed for a skill."""

    hashtags: tuple[str, ...]
    parent_bug: str


SKILL_CONFIGS = {
    'accessibility-cleanup': SkillConfig(
        hashtags=('Accessibility Cleanup',),
        parent_bug='531299755',
    ),
    'fix-outdated-javadocs': SkillConfig(
        hashtags=('Code Health', 'LSC'),
        parent_bug='564124183',
    ),
    'histogram-cleanup': SkillConfig(
        hashtags=('histogram-cleanup', 'LSC'),
        parent_bug='499059525',
    ),
    'histogram-extension': SkillConfig(
        hashtags=('Code Health',),
        parent_bug='499059543',
    ),
    'java-inline-fqn-cleanup': SkillConfig(
        hashtags=('Code Health', 'LSC'),
        parent_bug='528570333',
    ),
    'lint-sync': SkillConfig(
        hashtags=('lint-sync', 'LSC'),
        parent_bug='498775295',
    ),
    'remove-unused-object-overrides': SkillConfig(
        hashtags=('Code Health', 'LSC'),
        parent_bug='544923924',
    ),
    'replace-anonymous-runnables': SkillConfig(
        hashtags=('Code Health', 'LSC'),
        parent_bug='541671762',
    ),
}

# A real git trailer, e.g. `Bug: 123`.
_FOOTER_LINE_RE = re.compile(r'^[A-Za-z][A-Za-z0-9-]*:\s.*$')
# Not a git trailer; lives in its own paragraph before the trailers.
# Matches `OBSOLETE_HISTOGRAMS=<msg>` and `OBSOLETE_HISTOGRAM[<name>]=<msg>`.
_OBSOLETE_HISTOGRAM_RE = re.compile(
    r'^OBSOLETE_HISTOGRAM(?:S=|\[[^\]]+\]=)\S.*$'
)
_LSC_DOC_RE = re.compile(r'\blsc\s+doc:', re.IGNORECASE)
_URL_RE = re.compile(r'https?://\S+')
# A single line that is just a URL, optionally preceded by a short label.
_URL_LABEL_LINE_RE = re.compile(
    r'^(?:[A-Za-z][A-Za-z0-9 .-]{0,30}:\s+)?https?://\S+$'
)


def dedupe_preserve_order(items):
    """Returns |items| without case-insensitive duplicates, in order."""
    unique = {}
    for item in items:
        unique.setdefault(item.lower(), item)
    return list(unique.values())


def _real_bugs(bugs):
    """Returns stripped |bugs| minus empty and 'none' (any case) entries."""
    bugs = [str(b or '').strip() for b in bugs]
    return [b for b in bugs if b and b.lower() != 'none']


def format_bug_footer(parent_bug=None, extra_bugs=()):
    """Returns the `Bug:` footer with the parent bug first."""
    bugs = dedupe_preserve_order(_real_bugs([parent_bug, *extra_bugs]))
    if not bugs:
        raise ValueError('At least one bug is required.')
    return 'Bug: ' + ', '.join(bugs)


def _wrap_paragraph(paragraph):
    """Wraps |paragraph| to MAX_LINE_LENGTH, keeping URL lines unbroken."""
    if _URL_LABEL_LINE_RE.match(paragraph):
        return paragraph
    # Tag lines are one per line and can't be wrapped.
    if all(_OBSOLETE_HISTOGRAM_RE.match(x) for x in paragraph.split('\n')):
        return paragraph
    return textwrap.fill(
        paragraph,
        width=MAX_LINE_LENGTH,
        break_long_words=False,
        break_on_hyphens=False,
    )


def generate_commit_message(
    title,
    description,
    hashtag=None,
    parent_bug=None,
    extra_bugs=(),
    extra_footers=(),
    forbid_lsc_tag=False,
):
    """Returns the commit message; raises ValueError if it is not valid.

    |hashtag| is a string or an iterable of strings. The subject
    (`[tag1][tag2] title` or `title`) is never truncated. `LSC Doc:` is
    always rejected; if |forbid_lsc_tag| is set, `[LSC]` in the title is
    rejected too.
    """
    title = (title or '').strip()
    if forbid_lsc_tag and '[lsc]' in title.lower():
        raise ValueError('[LSC] is not allowed for this skill.')
    if isinstance(hashtag, str):
        hashtag = [hashtag]
    tags = [t.strip() for t in (hashtag or ()) if t and t.strip()]
    if tags:
        # Drop tags the caller already put in the title to avoid duplicates.
        title = re.sub(r'^(?:\[[^\]]+\]\s*)+', '', title).strip()
    prefix = ''.join(f'[{t}]' for t in tags)
    subject = f'{prefix} {title}' if prefix and title else title
    paragraphs = [p.strip() for p in (description or '').split('\n\n')]
    paragraphs = [p for p in paragraphs if p]
    body = '\n\n'.join(_wrap_paragraph(p) for p in paragraphs)
    if not subject or '\n' in subject or '\r' in subject:
        raise ValueError('Subject must be a non-empty single line.')
    if not body:
        raise ValueError('Description must not be empty.')
    footers = [f.strip() for f in extra_footers if f.strip()]
    footers.append(format_bug_footer(parent_bug, extra_bugs))
    message = f'{subject}\n\n{body}\n\n' + '\n'.join(footers) + '\n'
    # Subject length/period and banned content are checked by validation.
    errors = validate_commit_message(
        message,
        required_hashtags=tags,
        forbid_lsc_tag=forbid_lsc_tag,
    )
    if errors:
        raise ValueError('\n'.join(errors))
    return message


def validate_commit_message(
    message,
    required_bugs=(),
    required_hashtags=(),
    forbid_lsc_tag=False,
):
    """Returns a list of error strings (empty if the message is valid)."""
    if not message.strip():
        return ['Commit message is empty.']
    errors = []
    lines = message.rstrip('\n').split('\n')
    if len(lines[0]) > MAX_LINE_LENGTH:
        errors.append(
            f'Subject is {len(lines[0])} chars (max {MAX_LINE_LENGTH}).'
        )
    if lines[0].rstrip().endswith('.'):
        errors.append('Subject must not end with a period.')
    if len(lines) > 1 and lines[1].strip():
        errors.append('Subject must be followed by a blank line.')

    # The last paragraph is a footer block if every line looks like a footer;
    # footer lines (e.g. long tags) are exempt from wrapping.
    paragraphs = message.strip('\n').split('\n\n')
    last = paragraphs[-1].split('\n')
    exempt = set()
    if len(paragraphs) > 1 and all(_FOOTER_LINE_RE.match(x) for x in last):
        exempt = set(last)
    for i, line in enumerate(lines, start=1):
        # A URL can't be wrapped, so allow it to cause the excess length.
        url = _URL_RE.search(line)
        url_excess_ok = bool(url) and (
            len(line) - len(url.group(0)) <= MAX_LINE_LENGTH
        )
        if (
            i > 1
            and len(line) > MAX_LINE_LENGTH
            and line not in exempt
            and not _OBSOLETE_HISTOGRAM_RE.match(line)
            and not url_excess_ok
        ):
            errors.append(
                f'Line {i} is {len(line)} chars '
                f'(max {MAX_LINE_LENGTH}): {line[:40]}...'
            )
        errors.extend(
            f"Line {i} contains banned tag '{t}'."
            for t in BANNED_TAGS
            if line.startswith(t)
        )
        if GENERATOR_DISCLAIMER in line:
            errors.append(f'Line {i} contains a generator disclaimer.')

    if not any(line.startswith('Bug:') for line in lines):
        errors.append("Footer is missing a 'Bug:' line.")
    for bug in _real_bugs(required_bugs):
        pattern = re.compile(rf'^Bug:.*\b{re.escape(bug)}\b')
        if not any(pattern.match(line) for line in lines):
            errors.append(f"Footer is missing 'Bug: {bug}'.")

    if required_hashtags:
        expected_prefix = ''.join(f'[{t}]' for t in required_hashtags) + ' '
        if not lines[0].startswith(expected_prefix):
            errors.append(
                f"Subject must start with '{expected_prefix}' (exact casing)."
            )
        elif lines[0][len(expected_prefix) :].lstrip().startswith('['):
            errors.append('Subject has an extra or duplicate [tag] prefix.')
    if _LSC_DOC_RE.search(message):
        errors.append("'LSC Doc:' is not allowed.")
    if forbid_lsc_tag and '[lsc]' in lines[0].lower():
        errors.append('Subject must not contain [LSC] for this skill.')
    return errors


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('--skill', choices=sorted(SKILL_CONFIGS), required=True)
    for name in ('validate', 'title', 'description', 'out-commit'):
        parser.add_argument('--' + name)
    for name in ('bug', 'footer'):
        parser.add_argument('--' + name, action='append', default=[])
    args = parser.parse_args(argv)
    cfg = SKILL_CONFIGS[args.skill]
    forbid_lsc_tag = 'LSC' not in cfg.hashtags
    try:
        if args.validate:
            with open(args.validate, encoding='utf-8') as f:
                errors = validate_commit_message(
                    f.read(),
                    [cfg.parent_bug, *args.bug],
                    required_hashtags=cfg.hashtags,
                    forbid_lsc_tag=forbid_lsc_tag,
                )
            print(
                '\n'.join(errors) or 'Commit message is valid.',
                file=sys.stderr if errors else sys.stdout,
            )
            return 1 if errors else 0

        description = args.description
        if description == '-' or (
            description is None and not sys.stdin.isatty()
        ):
            description = sys.stdin.read()
        message = generate_commit_message(
            args.title,
            description,
            cfg.hashtags,
            cfg.parent_bug,
            args.bug,
            args.footer,
            forbid_lsc_tag=forbid_lsc_tag,
        )
        if args.out_commit:
            with open(args.out_commit, 'w', encoding='utf-8') as f:
                f.write(message)
        else:
            print(message, end='')
        return 0
    except (ValueError, OSError, UnicodeDecodeError) as e:
        print(e, file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
