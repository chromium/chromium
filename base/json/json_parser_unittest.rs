use base::{JsonOptions, NewValueSlotForTesting, ValueSlotRef};
use rust_gtest_interop::prelude::*;

#[gtest(RustJsonParserTest, ChromiumExtensions)]
fn test_chromium_extensions() {
    let opts = JsonOptions::with_chromium_extensions(101);
    expect_eq!(opts.allow_trailing_commas, false);
    expect_eq!(opts.replace_invalid_characters, false);
    expect_eq!(opts.allow_comments, true);
    expect_eq!(opts.allow_control_chars, true);
    expect_eq!(opts.allow_vert_tab, true);
    expect_eq!(opts.allow_x_escapes, true);
    expect_eq!(opts.max_depth, 101);
}

#[gtest(RustJsonParserTest, DecodeJson)]
fn test_decode_json() {
    // Exhaustively tested by existing C++ JSON tests.
    // This test is almost pointless but it seems wise to have a single
    // Rust-side test for the basics.
    let options = JsonOptions {
        max_depth: 128,
        allow_trailing_commas: false,
        replace_invalid_characters: false,
        allow_comments: false,
        allow_control_chars: false,
        allow_vert_tab: false,
        allow_x_escapes: false,
    };
    let mut value_slot = NewValueSlotForTesting();
    base::decode_json(b"{ \"a\": 4 }", options, ValueSlotRef::from(&mut value_slot)).unwrap();
    expect_eq!(format!("{:?}", ValueSlotRef::from(&mut value_slot)), "{\n   \"a\": 4\n}\n");
}
