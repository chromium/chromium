// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use core::any::TypeId;
use icu_casemap::CaseMapper;
use icu_casemap::CaseMapperBorrowed;
use icu_experimental::transliterate::{
    provider::TransliteratorRulesV1, CustomTransliterator, RuleCollection, RuleCollectionProvider,
    Transliterator,
};
use icu_locale_core::LanguageIdentifier;
use icu_provider::prelude::*;

struct TransliteratorMultiSourceProvider<'a>(
    RuleCollectionProvider<'a, icu_properties::provider::Baked, icu_normalizer::provider::Baked>,
);

impl<'a, M> DataProvider<M> for TransliteratorMultiSourceProvider<'a>
where
    M: DataMarker,
    RuleCollectionProvider<'a, icu_properties::provider::Baked, icu_normalizer::provider::Baked>:
        DataProvider<M>,
{
    fn load(&self, req: DataRequest) -> Result<DataResponse<M>, DataError> {
        if TypeId::of::<M>() == TypeId::of::<TransliteratorRulesV1>() {
            let mut silent_req = req;
            silent_req.metadata.silent = true;
            if let Some(response) = DataProvider::<TransliteratorRulesV1>::load(
                &icu_experimental::provider::Baked,
                silent_req,
            )
            .allow_identifier_not_found()?
            {
                return Ok(DataResponse {
                    metadata: response.metadata,
                    payload: response.payload.dynamic_cast()?,
                });
            }
        }
        self.0.load(req)
    }
}

#[derive(Debug)]
struct LowercaseTransliterator<'a>(CaseMapperBorrowed<'a>);

impl CustomTransliterator for LowercaseTransliterator<'_> {
    fn transliterate(&self, input: &str, range: std::ops::Range<usize>) -> String {
        self.0.lowercase_to_string(&input[range], &LanguageIdentifier::default())
    }
}

pub fn make_transliterator_from_rules(rules: &str) -> Transliterator {
    let mut collection = RuleCollection::default();
    collection.register_source(
        &"und-Hira-t-und-kana".parse().unwrap(),
        "<error>".to_string(),
        ["Katakana-Hiragana"],
        false,
        true,
    );
    collection.register_source(
        &"und-Kana-t-und-hira".parse().unwrap(),
        "<error>".to_string(),
        ["Hiragana-Katakana"],
        false,
        true,
    );
    // Register Latin-ASCII so that the alias mapping gets added
    collection.register_source(
        &"und-t-und-latn-d0-ascii".parse().unwrap(),
        "<error>".to_string(),
        ["Latin-ASCII"],
        false,
        true,
    );
    // Register Lower so that the alias mapping gets added
    collection.register_source(
        &"und-t-und-x0-lower".parse().unwrap(),
        "<error>".to_string(),
        ["Any-Lower"],
        false,
        true,
    );
    // Now register our new transliterator
    collection.register_source(
        &"und-t-und-x0-lowascii".parse().unwrap(),
        // "::NFD; ::[:Nonspacing Mark:] Remove; ::Any-Lower; ::NFC; ::Latin-ASCII;".to_string(),
        "::NFD; ::[:Nonspacing Mark:] Remove; ::Any-Lower; ::NFC; ::Latin-ASCII;".to_string(),
        [],
        false,
        true,
    );
    // Now register our new transliterator
    collection.register_source(
        &"und-t-und-x0-custom".parse().unwrap(),
        // "::NFD; ::[:Nonspacing Mark:] Remove; ::Lower; ::NFC; ::Latin-ASCII;".to_string(),
        rules.to_string(),
        [],
        false,
        true,
    );
    let provider = TransliteratorMultiSourceProvider(collection.as_provider());
    Transliterator::try_new_with_override_unstable(
        &provider,
        &provider,
        &"und-t-und-x0-custom".parse().unwrap(),
        |locale| {
            if locale.normalizing_eq("und-t-und-x0-lower") {
                Some(Ok(Box::new(LowercaseTransliterator(CaseMapper::new()))))
            } else {
                None
            }
        },
    )
    .unwrap()
}
pub fn make_transliterator_from_locale(locale: &str) -> Transliterator {
    let l = locale.parse().unwrap();
    Transliterator::try_new(&l).unwrap()
}

#[test]
fn test_lower_ascii() {
    let t = make_transliterator("hi");
    let r = t.transliterate("ÎÑŢÉRÑÅŢÎÖÑÅĻÎŽÅŢÎÖÑ".to_string());
    assert_eq!(r, "internationalization");
}
