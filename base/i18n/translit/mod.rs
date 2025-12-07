// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
mod custom_transliterator;

pub use custom_transliterator::make_transliterator_from_locale;
pub use custom_transliterator::make_transliterator_from_rules;
