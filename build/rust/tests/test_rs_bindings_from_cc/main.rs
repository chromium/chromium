// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

fn main() {
    println!("Hello world!");
    println!("AddViaCc(100,42) = {}", ::self_contained_target_rs_api::AddViaCc(100, 42));
    println!("MultiplyViaCc(100,42) = {}", ::self_contained_target_rs_api::MultiplyViaCc(100, 42));
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_self_contained_target() {
        assert_eq!(100 + 42, ::self_contained_target_rs_api::AddViaCc(100, 42));
        assert_eq!(100 * 42, ::self_contained_target_rs_api::MultiplyViaCc(100, 42));
    }
}
