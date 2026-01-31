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

mod test_pub_crate {
    chromium::import! {
        pub(crate) "//build/rust/chromium_prelude:import_test_lib" as library;
    }
}

mod test_pub_super {
    chromium::import! {
        pub(super) "//build/rust/chromium_prelude:import_test_lib" as library;
    }
}

mod test_trailing_separator_is_optional {
    chromium::import! {
        "//build/rust/chromium_prelude:import_test_lib"  // no semi-colon here
    }

    pub fn import_test() {
        import_test_lib::import_test_lib();
    }
}

fn main() {
    test_direct::import_test();
    test_as::import_test();
    test_pub::library::import_test_lib();
    test_pub_super::library::import_test_lib();
    test_pub_crate::library::import_test_lib();
    test_trailing_separator_is_optional::import_test();
}
