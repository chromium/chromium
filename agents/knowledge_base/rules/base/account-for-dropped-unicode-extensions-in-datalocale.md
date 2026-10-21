---
name: base/account-for-dropped-unicode-extensions-in-datalocale
rubric: testing
severity: nit
---

**Rule.** When converting an ICU4X Locale to DataLocale (such as via
locale.into() for fallback or data lookups), account for the fact that
DataLocale strips most Unicode BCP-47 extensions (such as -u-rg regional
overrides, retaining only -u-sd). Explicitly document with a comment or unit
test that dropping extension keywords is intended, or use LocalePreferences when
regional overrides must be preserved.

**Why.** In ICU4X, DataLocale is tailored for data lookups and CLDR fallback and
does not retain most Unicode BCP-47 extensions (such as -u-rg). A direct
conversion silently strips these extension keywords, potentially causing
unexpected fallback results if callers expect regional overrides to take
precedence.

## Examples

Bad:

```cpp
let mut fallbacks = Vec::new();
let mut iter = self.fallbacker.for_config(Default::default()).fallback_for(locale.into());
```

Good:

```cpp
let mut fallbacks = Vec::new();
// Conversion from Locale to DataLocale drops most extensions (except -u-sd).
let mut iter = self.fallbacker.for_config(Default::default()).fallback_for(locale.into());
```
