// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro::TokenStream;

#[proc_macro]
pub fn calculate_using_proc_macro(_item: TokenStream) -> TokenStream {
    "(15 + 15)".parse().unwrap()
}
