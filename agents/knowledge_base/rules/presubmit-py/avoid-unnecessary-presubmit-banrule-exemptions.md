---
name: presubmit-py/avoid-unnecessary-presubmit-banrule-exemptions
rubric: readability
severity: optional
---

**Rule.** When defining or updating `BanRule` entries in `PRESUBMIT.py` (such as
in `_BANNED_CPP_FUNCTIONS` or `_BANNED_JAVA_FUNCTIONS`), do not include path
exemptions in `excluded_paths` unless specific files or directories genuinely
require an exception. Avoid copying `excluded_paths` lists or tuples from
neighboring ban rules without verifying that the new rule actually requires
exemptions.

**Why.** Copying path exemptions from adjacent ban rules weakens presubmit
enforcement by allowing banned or discouraged APIs in files that do not need an
exception. Exemptions should only exist for legacy callers or tests that cannot
be migrated immediately.

## Examples

Bad:

```cpp
BanRule(
    r'/\bperfetto::Track::FromPointer',
    ('Creating tracks from pointer is discouraged because it risks aliasing. ',
     'Consider using NamedTrack instead.'),
    False,
    (
        r'^base/trace_event/.*',
        r'^base/tracing/.*',
    ),
),
```

Good:

```cpp
BanRule(
    r'/\bperfetto::Track::FromPointer',
    ('Creating tracks from pointer is discouraged because it risks aliasing. ',
     'Consider using NamedTrack instead.'),
    False,
    (),
),
```
