---
name: base/do-not-maximize-locales-for-icu4x-fallback
rubric: testing
severity: should_fix
---

**Rule.** When implementing locale matching or lookup using ICU4X
LocaleFallbacker, do not manually maximize preferred locales using
LocaleExpander and retry matching upon failure. Ensure supported locales are
stored or indexed in minimal form (e.g., 'sr' rather than 'sr-Cyrl'), or
construct an explicit directed fallback preference graph with pre-populated
aliases.

**Why.** ICU4X's LocaleFallbacker is designed on the invariant that data is
stored in minimal subtag form, guaranteeing that minimal representations are
reachable along the standard fallback chain. Manually maximizing preferred
locales with LocaleExpander and re-running fallback bypasses the fallback
hierarchy and can result in incorrect or suboptimal locale matches.

## Examples

Bad:

```cpp
// BAD: Retrying matching with a manually maximized locale when fallback fails.
int32_t matched_idx = matcher_->match_locale(preferred_str);
if (matched_idx != -1) {
  return supported_locales_[static_cast<size_t>(matched_idx)];
}

std::string maximized_str;
if (expander_->maximize(preferred_str, maximized_str)) {
  matched_idx = matcher_->match_locale(maximized_str);
  if (matched_idx != -1) {
    return supported_locales_[static_cast<size_t>(matched_idx)];
  }
}
```

Good:

```cpp
// GOOD: Supported locales are indexed in minimal form, or structured in a fallback graph.
for (const auto& fallback_locale :
     GetFallbackLocales(icu_fallbacker, preferred_locale)) {
  auto it = supported_locales_map_.find(fallback_locale);
  if (it != supported_locales_map_.end()) {
    return it->second;
  }
}
```
