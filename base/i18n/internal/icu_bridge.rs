// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use icu_locale::fallback::LocaleFallbacker;
use icu_locale::LocaleCanonicalizer;
use icu_locale_core::Locale;
use std::fmt;

#[cxx::bridge(namespace = "base::i18n::internal")]
pub mod ffi {
    pub struct OptionalIcu4xLocale {
        pub value: Box<Icu4xLocale>,
        pub has_value: bool,
    }

    extern "Rust" {
        type IcuCanonicalizer;
        type Icu4xLocale;

        fn create_icu_canonicalizer() -> Box<IcuCanonicalizer>;
        fn canonicalize(self: &IcuCanonicalizer, locale_bytes: &[u8]) -> OptionalIcu4xLocale;

        fn language(self: &Icu4xLocale) -> &str;
        fn script(self: &Icu4xLocale) -> &str;
        fn region(self: &Icu4xLocale) -> &str;
        fn variants(self: &Icu4xLocale) -> Vec<String>;
        fn extensions_as_strings(self: &Icu4xLocale) -> Vec<String>;
        #[cxx_name = "to_string"]
        fn to_string_inherent(self: &Icu4xLocale) -> String;

        type IcuFallbacker;
        fn create_icu_fallbacker() -> Box<IcuFallbacker>;
        fn fallback_to_vec(self: &IcuFallbacker, locale_bytes: &[u8]) -> Vec<Icu4xLocale>;
    }
}

pub struct IcuCanonicalizer {
    canonicalizer: LocaleCanonicalizer,
}

impl IcuCanonicalizer {
    pub fn new() -> Box<Self> {
        Box::new(Self { canonicalizer: LocaleCanonicalizer::new_extended() })
    }

    pub fn canonicalize(&self, locale_bytes: &[u8]) -> ffi::OptionalIcu4xLocale {
        let mut locale: Locale = match Locale::try_from_utf8(locale_bytes) {
            Ok(l) => l,
            Err(_) => {
                return ffi::OptionalIcu4xLocale {
                    value: Box::new(Icu4xLocale { locale: Locale::try_from_str("und").unwrap() }),
                    has_value: false,
                }
            }
        };

        if locale_bytes.starts_with(b"x-") || locale_bytes.starts_with(b"X-") {
            return ffi::OptionalIcu4xLocale {
                value: Box::new(Icu4xLocale { locale: Locale::try_from_str("und").unwrap() }),
                has_value: false,
            };
        }

        self.canonicalizer.canonicalize(&mut locale);
        ffi::OptionalIcu4xLocale { value: Box::new(Icu4xLocale { locale }), has_value: true }
    }
}

pub struct Icu4xLocale {
    locale: Locale,
}

impl Icu4xLocale {
    pub fn language(&self) -> &str {
        self.locale.id.language.as_str()
    }

    pub fn script(&self) -> &str {
        match self.locale.id.script {
            Some(ref s) => s.as_str(),
            None => "",
        }
    }

    pub fn region(&self) -> &str {
        match self.locale.id.region {
            Some(ref r) => r.as_str(),
            None => "",
        }
    }

    pub fn variants(&self) -> Vec<String> {
        self.locale.id.variants.iter().map(|v| v.to_string()).collect()
    }

    pub fn extensions_as_strings(&self) -> Vec<String> {
        let mut result = Vec::new();
        let ext = &self.locale.extensions;

        for other in ext.other.iter() {
            result.push(other.to_string());
        }

        if !ext.transform.is_empty() {
            result.push(ext.transform.to_string());
        }

        if !ext.unicode.is_empty() {
            result.push(ext.unicode.to_string());
        }

        result.sort();

        if !ext.private.is_empty() {
            result.push(ext.private.to_string());
        }

        result
    }

    pub fn to_string_inherent(&self) -> String {
        self.locale.to_string()
    }
}

impl fmt::Display for Icu4xLocale {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.locale)
    }
}

fn create_icu_canonicalizer() -> Box<IcuCanonicalizer> {
    IcuCanonicalizer::new()
}

pub struct IcuFallbacker {
    fallbacker: LocaleFallbacker,
}

impl IcuFallbacker {
    pub fn new() -> Box<Self> {
        Box::new(Self { fallbacker: LocaleFallbacker::new().static_to_owned() })
    }

    pub fn fallback_to_vec(&self, locale_bytes: &[u8]) -> Vec<Icu4xLocale> {
        let locale = match Locale::try_from_utf8(locale_bytes) {
            Ok(l) => l,
            Err(_) => {
                return Vec::new();
            }
        };

        let mut fallbacks = Vec::new();
        // Conversion from Locale to DataLocale drops most extensions (except -u-sd).
        let mut iter = self.fallbacker.for_config(Default::default()).fallback_for(locale.into());
        loop {
            let data_locale = iter.get();
            if data_locale.is_unknown() {
                break;
            }
            fallbacks.push(Icu4xLocale { locale: data_locale.into_locale() });
            iter.step();
        }

        fallbacks
    }
}

fn create_icu_fallbacker() -> Box<IcuFallbacker> {
    IcuFallbacker::new()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_canonicalizer_success() {
        let canonicalizer = IcuCanonicalizer::new();
        let res = canonicalizer.canonicalize(b"iw-IL");
        assert!(res.has_value);
        let loc = res.value;
        assert_eq!(loc.language(), "he");

        let script = loc.script();
        assert!(script.is_empty());

        let region = loc.region();
        assert!(!region.is_empty());
        assert_eq!(region, "IL");
    }

    #[test]
    fn test_canonicalizer_with_script() {
        let canonicalizer = IcuCanonicalizer::new();
        let res = canonicalizer.canonicalize(b"zh-Hant-TW");
        assert!(res.has_value);
        let loc = res.value;
        assert_eq!(loc.language(), "zh");

        let script = loc.script();
        assert!(!script.is_empty());
        assert_eq!(script, "Hant");

        let region = loc.region();
        assert!(!region.is_empty());
        assert_eq!(region, "TW");
    }

    #[test]
    fn test_canonicalizer_invalid_utf8() {
        let canonicalizer = IcuCanonicalizer::new();
        let res = canonicalizer.canonicalize(b"\xFF\xFE");
        assert!(!res.has_value);
        // The value should still be a valid object (und) but not "present"
        assert_eq!(res.value.language(), "und");
    }

    #[test]
    fn test_canonicalizer_private_use() {
        let canonicalizer = IcuCanonicalizer::new();
        let res = canonicalizer.canonicalize(b"x-private");
        assert!(!res.has_value);
        assert_eq!(res.value.language(), "und");
    }

    #[test]
    fn test_no_optional_components() {
        let canonicalizer = IcuCanonicalizer::new();
        let res = canonicalizer.canonicalize(b"en");
        assert!(res.has_value);
        let loc = res.value;
        assert_eq!(loc.language(), "en");
        assert!(loc.script().is_empty());
        assert!(loc.region().is_empty());
    }

    #[test]
    fn test_fallbacker() {
        let fallbacker = IcuFallbacker::new();
        let fallbacks = fallbacker.fallback_to_vec(b"en-US");
        let fallback_strings: Vec<String> = fallbacks.iter().map(|f| f.to_string()).collect();
        // ICU4X fallback typically produces ["en-US", "en"] for en-US.
        assert_eq!(fallback_strings, vec!["en-US", "en"]);

        let fallbacks_zh = fallbacker.fallback_to_vec(b"zh-Hant-TW");
        let fallback_zh_strings: Vec<String> = fallbacks_zh.iter().map(|f| f.to_string()).collect();
        // For zh-Hant-TW, it often includes script fallback.
        assert!(fallback_zh_strings.contains(&"zh-Hant".to_string()));

        let fallbacks_rg = fallbacker.fallback_to_vec(b"en-US-u-rg-gbeng");
        let fallback_rg_strings: Vec<String> = fallbacks_rg.iter().map(|f| f.to_string()).collect();
        // Currently, DataLocale ignores -u-rg extensions, so it falls back to en-US.
        assert_eq!(fallback_rg_strings, vec!["en-US", "en"]);
    }

    #[test]
    fn test_fallbacker_zh_hans() {
        let fallbacker = IcuFallbacker::new();
        let fallbacks = fallbacker.fallback_to_vec(b"zh-Hans");
        let fallback_strings: Vec<String> = fallbacks.iter().map(|f| f.to_string()).collect();
        // ICU4X fallback strips away the "Hans" script as it is the default for 'zh'.
        assert_eq!(fallback_strings, vec!["zh"]);
    }
}
