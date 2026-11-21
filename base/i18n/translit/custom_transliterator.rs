// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use std::borrow::Cow;

use icu_casemap::CaseMapperBorrowed;
use icu_experimental::transliterate::Transliterator;
use icu_locale_core::{LanguageIdentifier, Locale};
use icu_normalizer::{ComposingNormalizerBorrowed, DecomposingNormalizerBorrowed};

/// Single transliteration step resolved from an ICU transform ID or a
/// pre-baked BCP-47-T rule table in `//third_party/rust/icu4x_data`.
enum Step {
    RuleBased(Box<Transliterator>),
    Lower,
    Upper,
    Nfc,
    Nfd,
    Nfkc,
    Nfkd,
    Remove,
    Null,
}

impl Step {
    fn from_baked_bcp47(bcp47_id: &str) -> Option<Self> {
        let loc: Locale = bcp47_id.parse().ok()?;
        Transliterator::try_new(&loc).ok().map(Box::new).map(Self::RuleBased)
    }

    fn from_single_id(raw_id: &str) -> Option<Self> {
        let id = raw_id.trim();
        if id.is_empty() {
            return None;
        }
        let lower = id.to_ascii_lowercase();
        match lower.as_str() {
            "lower" | "any-lower" => Some(Self::Lower),
            "upper" | "any-upper" => Some(Self::Upper),
            "nfc" | "any-nfc" => Some(Self::Nfc),
            "nfd" | "any-nfd" => Some(Self::Nfd),
            "nfkc" | "any-nfkc" => Some(Self::Nfkc),
            "nfkd" | "any-nfkd" => Some(Self::Nfkd),
            "remove" | "any-remove" => Some(Self::Remove),
            "null" | "any-null" => Some(Self::Null),
            "latin-ascii" => Self::from_baked_bcp47("und-t-und-latn-d0-ascii"),
            // BCP-47-T orders `<target>-t-<source>`, whereas legacy ICU IDs
            // order `<source>-<target>`.
            "katakana-hiragana" | "kana-hira" => Self::from_baked_bcp47("und-hira-t-und-kana"),
            "hiragana-katakana" | "hira-kana" => Self::from_baked_bcp47("und-kana-t-und-hira"),
            // Direct BCP-47-T IDs baked into `//third_party/rust/icu4x_data`
            // (including Chromium's `und-t-und-x0-*` custom rule sets).
            bcp47 if !bcp47.contains('_') && bcp47.contains("-t-") => Self::from_baked_bcp47(bcp47),
            _ => None,
        }
    }

    fn apply(&self, input: String) -> String {
        match self {
            Self::RuleBased(t) => t.transliterate(input),
            Self::Lower => {
                match CaseMapperBorrowed::new()
                    .lowercase_to_string(&input, &LanguageIdentifier::UNKNOWN)
                {
                    Cow::Borrowed(_) => input,
                    Cow::Owned(s) => s,
                }
            }
            Self::Upper => {
                match CaseMapperBorrowed::new()
                    .uppercase_to_string(&input, &LanguageIdentifier::UNKNOWN)
                {
                    Cow::Borrowed(_) => input,
                    Cow::Owned(s) => s,
                }
            }
            Self::Nfc => match ComposingNormalizerBorrowed::new_nfc().normalize(&input) {
                Cow::Borrowed(_) => input,
                Cow::Owned(s) => s,
            },
            Self::Nfd => match DecomposingNormalizerBorrowed::new_nfd().normalize(&input) {
                Cow::Borrowed(_) => input,
                Cow::Owned(s) => s,
            },
            Self::Nfkc => match ComposingNormalizerBorrowed::new_nfkc().normalize(&input) {
                Cow::Borrowed(_) => input,
                Cow::Owned(s) => s,
            },
            Self::Nfkd => match DecomposingNormalizerBorrowed::new_nfkd().normalize(&input) {
                Cow::Borrowed(_) => input,
                Cow::Owned(s) => s,
            },
            Self::Remove => String::new(),
            Self::Null => input,
        }
    }
}

/// Sequence of one or more [`Step`]s executed in order.
pub(super) struct TransliteratorChain {
    steps: Vec<Step>,
}

impl TransliteratorChain {
    /// Resolve a single or `;`-chained transform ID (e.g. `"Lower"`,
    /// `"Latin-ASCII"`, `"Lower;Latin-ASCII"`, `"Katakana-Hiragana"`, or
    /// `"und-t-und-x0-afill"`).
    pub(super) fn try_from_id(id: &str) -> Option<Self> {
        let steps: Vec<Step> = id
            .split(';')
            .filter(|part| !part.trim().is_empty())
            .map(Step::from_single_id)
            .collect::<Option<Vec<_>>>()?;
        if steps.is_empty() {
            return None;
        }
        Some(Self { steps })
    }

    pub(super) fn transliterate(&self, input: String) -> String {
        self.steps.iter().fold(input, |acc, step| step.apply(acc))
    }
}
