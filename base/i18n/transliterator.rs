// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#[path = "translit/custom_transliterator.rs"]
mod custom_transliterator;

use custom_transliterator::TransliteratorChain;

#[cxx::bridge(namespace = "base::i18n::translit_ffi")]
mod ffi {
    extern "Rust" {
        type TransliteratorWrapper;
        fn create_from_id(id: &[u8]) -> Box<TransliteratorWrapper>;
        fn is_valid(t: &TransliteratorWrapper) -> bool;
        fn transliterate(t: &TransliteratorWrapper, text: &str) -> String;
    }
}

struct TransliteratorWrapper(Option<TransliteratorChain>);

fn create_from_id(id: &[u8]) -> Box<TransliteratorWrapper> {
    let inner = std::str::from_utf8(id).ok().and_then(TransliteratorChain::try_from_id);
    Box::new(TransliteratorWrapper(inner))
}

fn is_valid(t: &TransliteratorWrapper) -> bool {
    t.0.is_some()
}

fn transliterate(t: &TransliteratorWrapper, text: &str) -> String {
    match &t.0 {
        Some(chain) => chain.transliterate(text.to_string()),
        None => text.to_string(),
    }
}
