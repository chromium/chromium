// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use icu_locale::LocaleCanonicalizer;
use icu_locale_core::Locale;

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
}

fn create_icu_canonicalizer() -> Box<IcuCanonicalizer> {
    IcuCanonicalizer::new()
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
}
