// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

include!(concat!(env!("OUT_DIR"), "/generated/generated.rs"));

pub fn say_hello_from_crate() {
    assert_eq!(run_some_generated_code(), 42);
    #[cfg(is_new_rustc)]
    println!("Is new rustc!");
    #[cfg(is_old_rustc)]
    println!("Is old rustc!");
    #[cfg(is_android)]
    println!("Is android!");
    #[cfg(is_mac)]
    println!("Is darwin!");
    #[cfg(has_feature_a)]
    println!("Has feature A!");
    #[cfg(not(has_feature_a))]
    panic!("Wasn't passed feature a");
    #[cfg(not(has_feature_b))]
    #[cfg(test_a_and_b)]
    panic!("Wasn't passed feature b");
    #[cfg(has_feature_b)]
    #[cfg(not(test_a_and_b))]
    panic!("Was passed feature b");
}

#[cfg(test)]
mod tests {
    /// Test features are passed through from BUILD.gn correctly. This test is
    /// the target1 configuration.
    #[test]
    #[cfg(test_a_and_b)]
    fn test_features_passed_target1() {
        #[cfg(not(has_feature_a))]
        panic!("Wasn't passed feature a");
        #[cfg(not(has_feature_b))]
        panic!("Wasn't passed feature b");
    }

    /// This tests the target2 configuration is passed through correctly.
    #[test]
    #[cfg(not(test_a_and_b))]
    fn test_features_passed_target2() {
        #[cfg(not(has_feature_a))]
        panic!("Wasn't passed feature a");
        #[cfg(has_feature_b)]
        panic!("Was passed feature b");
    }

    #[test]
    fn test_generated_code_works() {
        assert_eq!(crate::run_some_generated_code(), 42);
    }
}
