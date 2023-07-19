// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod more;

fn main() {
    println!("Hello, world!");
    #[cfg(is_new_rustc)]
    println!("Is new rustc!");
    #[cfg(is_old_rustc)]
    println!("Is old rustc!");
    #[cfg(is_android)]
    println!("Is android!");
    #[cfg(is_mac)]
    println!("Is darwin!");

    more::hello();
}
