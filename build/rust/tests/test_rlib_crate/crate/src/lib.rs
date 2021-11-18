// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

pub fn say_hello_from_crate() {
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
    panic!("Wasn't passed feature b");
}
