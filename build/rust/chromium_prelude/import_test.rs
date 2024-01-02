// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

mod test_direct {
    chromium::import! {
        "//build/rust/chromium_prelude:import_test_lib";
    }

    pub fn import_test() {
        import_test_lib::import_test_lib();
    }
}

mod test_as {
    chromium::import! {
        "//build/rust/chromium_prelude:import_test_lib" as library;
    }

    pub fn import_test() {
        library::import_test_lib();
    }
}

mod test_pub {
    chromium::import! {
        pub "//build/rust/chromium_prelude:import_test_lib" as library;
    }
}

fn main() {
    test_direct::import_test();
    test_as::import_test();
    test_pub::library::import_test_lib();
}
