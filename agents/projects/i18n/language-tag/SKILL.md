---
name: language-tag
description: >-
  Guides usage of base::i18n::LanguageTag in Chromium C++, Mojo, and
  Preferences. Use when parsing, canonicalizing, storing, matching, or
  refactoring BCP47 language tags and locale strings.
---

# `base::i18n::LanguageTag`

`base::i18n::LanguageTag` (`base/i18n/language_tag.h`) is Chromium's immutable,
type-safe wrapper for canonical BCP47 language tags (e.g., `"en-US"`,
`"zh-Hant-TW"`, `"es-419"`, `"en-US-u-ca-gregory"`).

## Properties and GN Dependencies

- **Non-default-constructible**: `LanguageTag` has no public default constructor
  (only `mojo::DefaultConstruct::Tag` for Mojo deserialization and
  `base::PassKey<blink::HashTraits<LanguageTag>>` for Blink hash sentinels).
  Initialize variables/members explicitly or wrap in
  `std::optional<LanguageTag>`.
- **Value semantics**: Copyable, movable, `constexpr`-friendly, comparable with
  `==` and `<=>` (`std::strong_ordering` over `tag_string()`), streamable via
  `operator<<`, and hashable via `AbslHashValue`. Works in
  `base::MakeFixedFlatSet`, `base::MakeFixedFlatMap`, and `absl::flat_hash_map`.
- **GN targets**:
  - `//base/i18n:language_tag`: Core `LanguageTag`, `tag_converters`,
    `bcp47_extensions`, `language_tag_list_helper`, `language_tag_matcher`,
    `language_tag_value_converters`, `legacy_language_tag_helpers`,
    `locale_holder`, and `chinese_helpers`. Uses ICU4X locale canonicalization
    (Rust `icu_capi`) without depending on ICU4C.
  - `//base:i18n`: Full i18n target (`public_deps` on `//base/i18n:language_tag`
    and `//base/i18n/icubridge`), adding `IcuLocaleConverter`, `IcuBridge`,
    `GetDefaultIcuLocale`, and Android locale helpers.

## Creating and Parsing `LanguageTag`

### 1. Compile-Time Known Literals (`GetKnownLanguageTag`)

Use `base::i18n::GetKnownLanguageTag(std::string_view)`
(`base/i18n/language_tag.h`) for compile-time constants. This `consteval`
function validates BCP47 syntax, canonical casing (`en` lowercase, `Latn`
titlecase, `US` uppercase, `oxendict` lowercase), and membership in Chromium's
known subtags table (`base/i18n/internal/bcp47_known_subtags.h`). Extensions
(`u-`, `x-`) are not supported here.

```cpp
#include "base/i18n/language_tag.h"

constexpr base::i18n::LanguageTag kDefaultTag =
    base::i18n::GetKnownLanguageTag("und");
constexpr base::i18n::LanguageTag kLongKnownTag =
    base::i18n::GetKnownLanguageTag("en-Latn-GB-oxendict");
```

### 2. Runtime String Parsing (`GetLanguageTagFromString`)

Use `base::i18n::GetLanguageTagFromString(std::string_view)`
(`base/i18n/tag_converters.h`) rather than calling
`LanguageTagConverter::GetInstance().FromString(...)` directly. It parses and
canonicalizes via ICU4X, returning `std::optional<LanguageTag>`:

- Normalizes casing and separators (`"EN-us"` -> `"en-US"`, `"en_US"` ->
  `"en-US"`).
- Canonicalizes deprecated tags (`"iw"` -> `"he"`, `"cmn"` -> `"zh"`, `"tl"` ->
  `"fil"`, `"deu"`/`"ger"` -> `"de"`, Windows `"zh-chs"` -> `"zh-CN"`). Legacy
  `"sh"` is intentionally preserved without canonicalization.
- Converts legacy ICU keywords and strips POSIX encodings
  (`"en_US.UTF8@currency=USD"` -> `"en-US-u-cu-usd"`, `"ca_ES@valencia"` ->
  `"ca-ES-u-va-valencia"`).
- Returns `std::nullopt` for strings shorter than 2 chars, malformed tags, or
  standalone private-use tags (`"x-private"`; `"und-x-private"` and
  `"en-x-test"` are valid).

```cpp
#include "base/i18n/tag_converters.h"

std::optional<base::i18n::LanguageTag> tag =
    base::i18n::GetLanguageTagFromString(raw_locale);
```

### 3. Comma-Separated Strings and String Lists

Use `base/i18n/language_tag_list_helper.h` instead of manually splitting and
parsing locale lists (such as accept-languages strings):

- `GetLanguageTagListFromString(std::string_view[, filter])`: Splits on `','`,
  trims whitespace, parses each element via `GetLanguageTagFromString`, and
  skips empty or malformed items while preserving order and duplicates.
- `GetLanguageTagListFromStringList(base::span<const std::string>[, filter])`:
  Parses each element via `GetLanguageTagFromString`, skipping malformed items.
- `LanguageTagListToString(base::span<const LanguageTag>)`: Joins tags with
  `','` and no whitespace.

## Inspecting Subtags, Parent Chains, and Extensions

### Subtags and Hierarchy (`base/i18n/language_tag.h`)

- `tag_string()` -> `std::string_view` (`LIFETIME_BOUND`): Canonical BCP47 tag.
- `language_subtag()` -> `std::string_view` (`LIFETIME_BOUND`): Base language.
- `WithLanguageSubtagOnly()` -> `LanguageTag`: Tag with only
  `language_subtag()`.
- `script_subtag()` -> `std::string_view` (`LIFETIME_BOUND`): Script or `""`.
- `region_subtag()` -> `std::string_view` (`LIFETIME_BOUND`): Region or `""`.
- `variant_subtags()` -> `std::vector<std::string_view>` (`LIFETIME_BOUND`):
  Sorted variants.
- `GetParentTag()` -> `std::optional<LanguageTag>` (`constexpr`): Strips the
  most specific component in order: private-use (`x-`) -> all non-private
  extensions -> last variant -> region -> script. Returns `std::nullopt` when
  only the base language remains.
- `GetLineage()` -> `std::vector<LanguageTag>` (`constexpr`): Returns the full
  parent chain starting with `*this` (e.g., `"sr-Latn-RS"` ->
  `["sr-Latn-RS", "sr-Latn", "sr"]`).
- `base/i18n/chinese_helpers.h`: `IsChinese(tag)`, `IsSimplifiedChinese(tag)`,
  and `IsTraditionalChinese(tag)`.

### Extensions and Private-Use Subtags (`base/i18n/bcp47_extensions.h`)

Retrieve extensions with `GetExtension(...)` and produce updated tags
(immutably) with `WithExtension(...)` or `WithExtensionRemoved(char key)`:

- Unicode (`u-`): `tag.GetExtension(bcp47_extensions::unicode())` ->
  `std::optional<UnicodeExtension>` (`has_attribute`, `AddAttribute`,
  `GetKeywordValue`, `SetKeyword`, `keywords`, `GetKeywordKeys`).
- Private-use (`x-`): `tag.GetExtension(bcp47_extensions::priv())` ->
  `std::optional<PrivateUseSubtags>` (`AddSubtag`, `SubtagsString`).
- Other singletons: `tag.GetExtension(bcp47_extensions::ext<'a'>())` ->
  `std::optional<Extension>` (`singleton`, `AddSubtag`, `SubtagsString`).

```cpp
#include "base/i18n/bcp47_extensions.h"

if (auto u_ext = tag.GetExtension(base::i18n::bcp47_extensions::unicode())) {
  u_ext->SetKeyword("ca", "gregory");
  tag = tag.WithExtension(*u_ext);
}
base::i18n::LanguageTag no_unicode = tag.WithExtensionRemoved('u');
```

## Matching, Thread-Safe Holders, and Chromium Integrations

### Language Matching (`base/i18n/language_tag_matcher.h`)

`LanguageTagMatcher` precomputes an ICU4X fallback graph (including script and
macro-region affinity, e.g., `"zh-HK"` -> `"zh-TW"`, `"es-MX"` -> `"es-419"`).

- Construct once and reuse (`LanguageTagMatcher::Create` is expensive):
  - `LanguageTagMatcher::Create(supported_tags)` with `Match(preferred_tag)` ->
    `std::optional<LanguageTag>` and `HasExactMatch(preferred_tag)`.
  - `LanguageTagMatcherWithDefault::Create(default_tag, supported_tags)` with
    `MatchOrDefault(preferred_tag)` -> `LanguageTag`.
- For Chromium's built-in locale lists, reuse the cached singletons in
  `ui/base/l10n/chromium_language_matcher.h`: `GetAcceptLanguageMatcher()`,
  `GetAcceptLanguageTags()`, `GetPlatformLanguageMatcher()`, and
  `GetPlatformLanguageTags()`.

### Thread-Safe Holders and `//base:i18n` / `//ui/base/l10n` APIs

- `base/i18n/locale_holder.h`:
  - `ThreadSafeLocaleHolder`: Lock-protected `GetLocale()`, `SetLocale()`, and
    `RegisterOnLocaleChangedCallback()`.
  - `SequenceCheckedLocaleHolder`: Sequence-checked `SetLocale()` with
    lock-protected `GetLocale()` readable from any thread.
- `base/i18n/icubridge/default_icu_locale.h`: `GetDefaultIcuLocale()` and
  `RegisterOnDefaultLocaleUpdatedCallback()`. Always use `GetDefaultIcuLocale()`
  instead of `icu::Locale::getDefault()`, which must not be used outside of
  `base/i18n`; if an `icu::Locale` is needed, convert the returned `LanguageTag`
  with `IcuLocaleConverter::GetInstance().FromLanguageTag()`.
  (`SetDefaultIcuLocale` is pass-key restricted via
  `DefaultIcuLocaleSetterKey`.)
- `base/i18n/icubridge/icu_bridge.h`:
  `IcuBridge::GetInstance().date_time_formatter(locale)` accepts
  `const LanguageTag&`.
- `base/i18n/icu4c_tag_converter.h`: `IcuLocaleConverter::GetInstance()`
  provides `FromLanguageTag(tag)` and `ToLanguageTag(icu_locale)` for legacy
  `icu::Locale` boundaries.
- `base/i18n/android_locale.h`: `GetAndroidDefaultLocale()` returns
  `LanguageTag`.
- `ui/base/l10n/l10n_util.h`: Prefer the `LanguageTag` overloads when available
  (`GetDisplayNameForLocale`, `GetAcceptLanguageTagsForLocale`,
  `IsAcceptLanguageDisplayable`, `KeepAcceptedLanguages`,
  `GetPlatformLocalesForTesting`).

### Legacy String Interop

- `LanguageTag::ToLegacyICUFormat()` returns underscore format (`"en_US"`,
  `"de_DE@calendar=gregorian;collation=phonebook"`), ignoring non-Unicode
  extensions. Avoid calling it for comparisons or internal APIs; keep
  `LanguageTag` objects instead.
- `base/i18n/legacy_language_tag_helpers.h`:
  `GetLanguageSubtagUsingLanguageTag(std::string_view)`.

## Mojo (`mojo_base.mojom.LanguageTag`)

`mojo_base.mojom.LanguageTag` (`mojo/public/mojom/base/language_tag.mojom`) is
typemapped (`default_constructible = false`) to `::base::i18n::LanguageTag` in
both standard Chromium and Blink C++ Mojo bindings:

```mojom
import "mojo/public/mojom/base/language_tag.mojom";

struct LocaleConfig {
  mojo_base.mojom.LanguageTag language;
};
```

- On deserialization (`mojo/public/cpp/base/language_tag_mojom_traits.cc`),
  `tag_string` is parsed via `LanguageTagConverter::FromString`; empty or
  malformed tags fail validation and drop the message.
- In Blink, `mojo/public/mojom/base/BUILD.gn` automatically pulls in
  `third_party/blink/renderer/platform/wtf/base_hash_traits.h` so `LanguageTag`
  works as a `blink::HashMap` key. For direct `base::i18n::LanguageTag` usage in
  Blink, verify the directory is allowlisted in
  `third_party/blink/tools/blinkpy/presubmit/audit_non_blink_usage.py`.

## Preferences and `base::Value`

- **Registration** (`components/prefs/pref_registry_simple.h`):
  ```cpp
  registry->RegisterLanguageTagPref(
      prefs::kPreferredLanguage, base::i18n::GetKnownLanguageTag("en"));
  ```
- **Read/Write** (`components/prefs/pref_service.h`):
  - `pref_service->GetLanguageTag(path)` (falls back to
    `GetKnownLanguageTag("und")` if the stored value is missing/invalid).
  - `pref_service->SetLanguageTag(path, tag)`.
- **PrefMember** (`components/prefs/pref_member_language_tag.h` — defined in its
  own header, separate from `pref_member.h`, to keep `pref_member.h` compile
  costs low):
  ```cpp
  #include "components/prefs/pref_member_language_tag.h"

  LanguageTagPrefMember preferred_language_;
  ```
- **`base::Value` converters** (`base/i18n/language_tag_value_converters.h`):
  `LanguageTagToValue(tag)` and `ValueToLanguageTag(value)`.
  `ValueToLanguageTag` parses string values via `GetLanguageTagFromString()`
  (canonicalizing inputs such as `"en_US"` -> `"en-US"` and `"iw"` -> `"he"`)
  and returns `std::nullopt` for null, non-string, or malformed values.

## Testing Utilities

- `base/i18n/test/locales_for_test.h`: `GetLocalesForTest()` returns a test list
  of `LanguageTag`s.
- `base/i18n/test/scoped_icu_locale.h`:
  `ScopedDefaultIcuLocale(const LanguageTag&)` overrides and restores
  `GetDefaultIcuLocale()` (do not use `icu::Locale::getDefault()` or
  `icu::Locale::setDefault()` directly).
- `ui/base/l10n/test/scoped_application_locale.h` and
  `ui/base/l10n/test/scoped_resource_bundle_locale.h`: Scoped locale overrides
  accepting `const base::i18n::LanguageTag&`.
