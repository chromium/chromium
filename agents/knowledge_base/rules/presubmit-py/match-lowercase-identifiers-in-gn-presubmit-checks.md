---
name: presubmit-py/match-lowercase-identifiers-in-gn-presubmit-checks
rubric: correctness
severity: should_fix
---

**Rule.** When writing presubmit checks, BanRule patterns, or regular
expressions targeting GN build files (.gn, .gni), match against
lower_case_with_underscores identifiers or use case-insensitive matching ((?i)).
Do not match against uppercase C++ preprocessor macro names (such as
IS_FEATURE_NAME), as GN variables and build arguments are strictly lowercase.

**Why.** GN variables and build arguments follow lower_case_with_underscores
naming conventions per the GN style guide, whereas C++ preprocessor macros use
ALL_CAPS. Presubmit checks matching uppercase identifiers against GN files
silently fail to detect usages of the intended build argument or variable.

## Examples

Bad:

```cpp
_BANNED_GN_PATTERNS: Sequence[BanRule] = (
    BanRule(
        pattern=r'IS_DESKTOP_ANDROID',
        explanation=(
            'Do not use IS_DESKTOP_ANDROID directly; use is_android instead.',
        ),
    ),
)
```

Good:

```cpp
_BANNED_GN_PATTERNS: Sequence[BanRule] = (
    BanRule(
        pattern=r'/(?i)is_desktop_android',
        explanation=(
            'Do not use is_desktop_android directly; use is_android instead.',
        ),
    ),
)
```
