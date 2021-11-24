// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro::TokenStream;

#[proc_macro]
pub fn say_hello_from_proc_macro(_item: TokenStream) -> TokenStream {
    "println!(\"Hello from proc macro\");".parse().unwrap()
}
