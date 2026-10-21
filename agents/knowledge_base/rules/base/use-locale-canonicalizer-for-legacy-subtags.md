---
name: base/use-locale-canonicalizer-for-legacy-subtags
rubric: safety
severity: should_fix
---

**Rule.** In ICU4X locale handling, distinguish syntax normalization from
semantic canonicalization: use Locale::normalize (or Locale::normalize_utf8)
instead of parsing into a full Locale struct and calling .to_string() when only
validating BCP-47 syntax and casing. Conversely, use LocaleCanonicalizer (or
icu4x::LocaleCanonicalizer) loaded with CLDR data rather than syntax-only
normalization when transforming deprecated, legacy, or alias subtags (e.g., 'iw'
to 'he').

**Why.** Locale::normalize operates directly on string slices and returns a
Cow<str>, avoiding intermediate AST allocation and parsing overhead for
syntax-only checks. However, syntax normalization only enforces BCP-47 grammar
and casing without consulting CLDR alias mappings; semantic subtag
canonicalization requires LocaleCanonicalizer.

## Examples

Bad:

```cpp
// Syntax-only normalization using expensive struct parsing and serialization:
fn normalize_syntax(locale: &str) -> String {
    match locale.parse::<Locale>() {
        Ok(loc) => loc.to_string(),
        Err(_) => String::new(),
    }
}

// Attempting semantic subtag canonicalization without LocaleCanonicalizer:
let mut loc = locale_str.parse::<Locale>().ok()?;
let canonical_str = loc.to_string(); // Fails to transform deprecated "iw" to "he"
```

Good:

```cpp
// Syntax-only normalization using Locale::normalize:
fn normalize_syntax(locale: &str) -> String {
    Locale::normalize(locale).map(|s| s.into_owned()).unwrap_or_default()
}

// Semantic subtag canonicalization with LocaleCanonicalizer:
let mut loc = Locale::try_from_utf8(locale_bytes).ok()?;
canonicalizer.canonicalize(&mut loc);
let canonical_str = loc.to_string();
```
