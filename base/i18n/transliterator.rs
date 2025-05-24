// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use icu_experimental::transliterate::Transliterator;

#[path = "translit/mod.rs"]
mod translit;

#[cxx::bridge(namespace = "transliterator")]
pub mod ffi {
    extern "Rust" {
        type TransliteratorWrapper;
        fn create_from_locale(locale: &str) -> Box<TransliteratorWrapper>;
        fn create_from_rules(rules: &str) -> Box<TransliteratorWrapper>;
        fn transliterate(t: &TransliteratorWrapper, text: &str) -> String;
    }
}

struct TransliteratorWrapper(Transliterator);

fn create_from_rules(rules: &str) -> Box<TransliteratorWrapper> {
    return Box::new(TransliteratorWrapper(translit::make_transliterator_from_rules(rules)));
}
fn create_from_locale(locale: &str) -> Box<TransliteratorWrapper> {
    return Box::new(TransliteratorWrapper(translit::make_transliterator_from_locale(locale)));
}

fn transliterate(t: &TransliteratorWrapper, text: &str) -> String {
    return t.0.transliterate(text.to_string());
}
