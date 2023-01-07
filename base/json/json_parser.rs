// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains bindings to convert from JSON to C++ base::Value objects.
// The code related to `base::Value` can be found in 'values.rs'
// and 'values_deserialization.rs'.

use crate::values::ValueSlotRef;
use crate::values_deserialization::ValueVisitor;
use cxx::CxxString;
use serde::de::Deserializer;
use serde_json_lenient::de::SliceRead;
use std::pin::Pin;

// UTF8 byte order mark.
const UTF8_BOM: [u8; 3] = [0xef, 0xbb, 0xbf];

#[cxx::bridge(namespace=base::rs_glue)]
mod ffi {
    /// Options for parsing JSON inputs. Largely a mirror of the C++ `base::JSONParserOptions`
    /// bitflags, represented as a friendlier struct-of-bools instead.
    #[namespace=base::ffi::json::json_parser]
    struct JsonOptions {
        /// Allows commas to exist after the last element in structures.
        allow_trailing_commas: bool,
        /// If set the parser replaces invalid code points (i.e. lone surrogates) with the Unicode
        /// replacement character (U+FFFD). If not set, invalid code points trigger a hard error and
        /// parsing fails.
        replace_invalid_characters: bool,
        /// Allows both C (/* */) and C++ (//) style comments.
        allow_comments: bool,
        /// Permits unescaped ASCII control characters (such as unescaped \r and \n) in the range
        /// [0x00,0x1F].
        allow_control_chars: bool,
        /// Permits \\v vertical tab escapes.
        allow_vert_tab: bool,
        /// Permits \\xNN escapes as described above.
        allow_x_escapes: bool,
        /// The maximum recursion depth to walk while parsing nested JSON objects. JSON beyond the
        /// specified depth will be ignored.
        max_depth: usize,
    }

    unsafe extern "C++" {
        include!("base/rs_glue/values_glue.h");
        type ValueSlot = crate::rs_glue::ffi::ValueSlot;
    }

    extern "Rust" {
        #[namespace=base::ffi::json::json_parser]
        pub fn decode_json_from_cpp(
            json: &[u8],
            options: JsonOptions,
            value_slot: Pin<&mut ValueSlot>,
            error_line: &mut i32,
            error_column: &mut i32,
            error_message: Pin<&mut CxxString>,
        ) -> bool;
    }
}

pub type JsonOptions = ffi::JsonOptions;
impl JsonOptions {
    /// Construct a JsonOptions with common Chromium extensions.
    ///
    /// Per base::JSONParserOptions::JSON_PARSE_CHROMIUM_EXTENSIONS:
    ///
    /// This parser historically accepted, without configuration flags,
    /// non-standard JSON extensions. This enables that traditional parsing
    /// behavior.
    pub fn with_chromium_extensions(max_depth: usize) -> JsonOptions {
        JsonOptions {
            allow_trailing_commas: false,
            replace_invalid_characters: false,
            allow_comments: true,
            allow_control_chars: true,
            allow_vert_tab: true,
            allow_x_escapes: true,
            max_depth,
        }
    }
}

/// Decode some JSON into C++ base::Value object tree.
///
/// This function takes and returns normal Rust types. For an equivalent which
/// can be called from C++, see `decode_json_from_cpp`.
///
/// # Args:
///
/// * `json`: the JSON. Note that this is a slice of bytes rather than a string,
///           which in Rust terms means it hasn't yet been validated to be
///           legitimate UTF8. The JSON decoding will do that.
/// * `options`: configuration options for non-standard JSON extensions
/// * `value_slot`: a space into which to construct a base::Value
///
/// It always strips a UTF8 BOM from the start of the string, if one is found.
///
/// Return: a serde_json_lenient::Error or Ok.
///
/// It is be desirable in future to expose this API to other Rust code inside
/// and outside //base. TODO(crbug/1287209): work out API norms and then add
/// 'pub' to do this.
pub fn decode_json(
    json: &[u8],
    options: JsonOptions,
    value_slot: ValueSlotRef,
) -> Result<(), serde_json_lenient::Error> {
    let mut to_parse = json;
    if to_parse.len() >= 3 && to_parse[0..3] == UTF8_BOM {
        to_parse = &to_parse[3..];
    }
    let mut deserializer = serde_json_lenient::Deserializer::new(SliceRead::new(
        &to_parse,
        options.replace_invalid_characters,
        options.allow_control_chars,
        options.allow_vert_tab,
        options.allow_x_escapes,
    ));
    // By default serde_json[rc] has a recursion limit of 128.
    // As we want different recursion limits in different circumstances,
    // we disable its own recursion tracking and use our own.
    deserializer.disable_recursion_limit();
    deserializer.set_ignore_trailing_commas(options.allow_trailing_commas);
    deserializer.set_allow_comments(options.allow_comments);
    // The C++ parser starts counting nesting levels from the first item
    // inside the outermost dict. We start counting from the
    // absl::optional<base::Value> and also count the outermost dict,
    // therefore we start with -2 to match C++ behavior.
    let result =
        deserializer.deserialize_any(ValueVisitor::new(value_slot, options.max_depth - 2))?;
    deserializer.end()?;
    Ok(result)
}

/// Decode some JSON into a `base::Value`; for calling by C++.
///
/// See `decode_json` for an equivalent which takes and returns idiomatic Rust
/// types, and a little bit more information about the implementation.
///
/// # Args
///
/// * `json`: a slice of input JSON unsigned characters.
/// * `options`: configuration options for non-standard JSON extensions
/// * `value_slot`:  a space into which to construct a base::Value
/// * `error_line`/`error_column`/`error_message`: populated with details of
///                                                any decode error.
///
/// # Returns
///
/// A Boolean indicating whether the decode succeeded.
fn decode_json_from_cpp(
    json: &[u8],
    options: ffi::JsonOptions,
    value_slot: Pin<&mut ffi::ValueSlot>,
    error_line: &mut i32,
    error_column: &mut i32,
    mut error_message: Pin<&mut CxxString>,
) -> bool {
    let value_slot = ValueSlotRef::from(value_slot);
    match decode_json(json, options, value_slot) {
        Err(err) => {
            *error_line = err.line().try_into().unwrap_or(-1);
            *error_column = err.column().try_into().unwrap_or(-1);
            error_message.as_mut().clear();
            // The following line pulls in a lot of binary bloat, due to all the formatter
            // implementations required to stringify error messages. This error message is used in
            // only a couple of places outside unit tests so we could consider trying
            // to eliminate.
            error_message.as_mut().push_str(&err.to_string());
            false
        }
        Ok(_) => true,
    }
}
