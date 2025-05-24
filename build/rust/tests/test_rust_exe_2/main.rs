// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

fn main() {
    assert_eq!(test_proc_macro_crate::calculate_using_proc_macro!(), 0);
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_proc_macro() {
        assert_eq!(test_proc_macro_crate::calculate_using_proc_macro!(), 0)
    }
}
