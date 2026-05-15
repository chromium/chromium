// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use cxx::CxxString;
use icu_locale::LocaleCanonicalizer;
use icu_locale_core::Locale;
use std::pin::Pin;

#[cxx::bridge(namespace = "base::i18n::internal")]
pub mod ffi {
    extern "Rust" {
        type IcuCanonicalizer;

        fn create_icu_canonicalizer() -> Box<IcuCanonicalizer>;

        fn create_canonical(&self, locale_bytes: &[u8], out: Pin<&mut CxxString>);
    }
}

pub struct IcuCanonicalizer {
    canonicalizer: LocaleCanonicalizer,
}

impl IcuCanonicalizer {
    pub fn new() -> Box<Self> {
        Box::new(Self { canonicalizer: LocaleCanonicalizer::new_extended() })
    }

    pub fn create_canonical(&self, locale_bytes: &[u8], mut out: Pin<&mut CxxString>) {
        out.as_mut().clear();
        let mut locale: Locale = match Locale::try_from_utf8(locale_bytes) {
            Ok(l) => l,
            Err(_) => return,
        };

        // Canonicalize (iw -> he, en-US-POSIX -> en-US-posix, etc.)
        self.canonicalizer.canonicalize(&mut locale);
        let s = locale.to_string();
        out.as_mut().push_str(&s);
    }
}

fn create_icu_canonicalizer() -> Box<IcuCanonicalizer> {
    IcuCanonicalizer::new()
}
