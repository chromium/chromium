---
name: watchlists/use-negative-lookahead-to-exclude-subdirectories-in-watchlis
rubric: correctness
severity: nit
---

**Rule.** In WATCHLISTS, use a negative lookahead assertion (such as
`(?!/subfolder)` or `(?!subfolder/)`) in a `filepath` regular expression when a
parent directory pattern should exclude specific subfolders or nested
components.

**Why.** WATCHLISTS matches filepaths via Python's `re.search()` substring
matching, meaning a parent path pattern automatically matches all nested
subdirectories. When a child directory is tracked by a separate watchlist or
should not notify parent watchers, a negative lookahead assertion prevents
unintended matches.

## Examples

Bad:

```cpp
    'ios_intelligence_actor': {
      'filepath': 'ios/chrome/browser/intelligence/actor'
    },
    'ios_intelligence_actor_tools': {
      'filepath': 'ios/chrome/browser/intelligence/actor/tools'
    },
```

Good:

```cpp
    'ios_intelligence_actor': {
      'filepath': 'ios/chrome/browser/intelligence/actor(?!/tools)'
    },
    'ios_intelligence_actor_tools': {
      'filepath': 'ios/chrome/browser/intelligence/actor/tools'
    },
```
