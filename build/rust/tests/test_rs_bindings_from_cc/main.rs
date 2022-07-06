// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

fn main() {
    println!("Hello world!");
    println!("MultiplyViaCc(100,42) = {}", ::self_contained_header_rs_api::MultiplyViaCc(100, 42));
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_call_via_crubit() {
        assert_eq!(100 * 42, ::self_contained_header_rs_api::MultiplyViaCc(100, 42));
    }
}
