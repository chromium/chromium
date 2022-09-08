// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use crate::values::{DictValueRef, ListValueRef, ValueSlotRef};
use serde::de::{DeserializeSeed, Deserializer, Error, MapAccess, SeqAccess, Visitor};
use std::convert::TryFrom;
use std::fmt;

/// Struct to check we're not recursing too deeply.
#[derive(Clone)]
struct RecursionDepthCheck(usize);

impl RecursionDepthCheck {
    /// Recurse a level and return an error if we've recursed too far.
    fn recurse<E: Error>(&self) -> Result<RecursionDepthCheck, E> {
        match self.0.checked_sub(1) {
            Some(recursion_limit) => Ok(RecursionDepthCheck(recursion_limit)),
            None => Err(Error::custom("recursion limit exceeded")),
        }
    }
}

/// What type of `base::Value` container we're deserializing into.
enum DeserializationTarget<'elem, 'container> {
    /// Deserialize into a brand new root `base::Value`.
    NewValue { slot: ValueSlotRef<'container> },
    /// Deserialize by appending to a list.
    List { list: &'elem mut ListValueRef<'container> },
    /// Deserialize by setting a dictionary key.
    Dict { dict: &'elem mut DictValueRef<'container>, key: String },
}

/// serde deserializer for for `base::Value`.
///
/// serde is the "standard" framework for serializing and deserializing
/// data formats in Rust. https://serde.rs/
///
/// This implements both the Visitor and Deserialize roles described
/// here: https://serde.rs/impl-deserialize.html
///
/// One note, though. Normally serde instantiates a new object. The design
/// of `base::Value` is that each sub-value (e.g. a list like this)
/// needs to be deserialized into the parent value, which is pre-existing.
/// To achieve this we use a feature of serde called 'stateful deserialization'
/// (see https://docs.serde.rs/serde/de/trait.DeserializeSeed.html)
///
/// This struct stores that state.
///
/// Handily, this also enables us to store the desired recursion limit.
///
/// We use runtime dispatch (matching on an enum) to deserialize into a
/// dictionary, list, etc. This may be slower than having three different
/// `Visitor` implementations, where everything would be monomorphized
/// and would probably disappear with inlining, but for now this is much
/// less code.
pub struct ValueVisitor<'elem, 'container> {
    container: DeserializationTarget<'elem, 'container>,
    recursion_depth_check: RecursionDepthCheck,
}

impl<'elem, 'container> ValueVisitor<'elem, 'container> {
    /// Constructor - pass in an existing slot that can store a new
    /// `base::Value`, then this visitor can be passed to serde deserialization
    /// libraries to populate it with a tree of contents.
    /// Any existing `base::Value` in the slot will be replaced.
    pub fn new(slot: ValueSlotRef<'container>, mut max_depth: usize) -> Self {
        max_depth += 1; // we will increment this counter when deserializing
        // the initial `base::Value`. To match C++ behavior, we should
        // only start counting for subsequent layers, hence decrement
        // now.
        Self {
            container: DeserializationTarget::NewValue { slot },
            recursion_depth_check: RecursionDepthCheck(max_depth),
        }
    }
}

impl<'de, 'elem, 'container> Visitor<'de> for ValueVisitor<'elem, 'container> {
    // Because we are deserializing into a pre-existing object.
    type Value = ();

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("any valid JSON")
    }

    fn visit_i32<E: serde::de::Error>(self, value: i32) -> Result<Self::Value, E> {
        match self.container {
            DeserializationTarget::NewValue { mut slot } => slot.construct_integer(value),
            DeserializationTarget::List { list } => list.append_integer(value),
            DeserializationTarget::Dict { dict, key } => dict.set_integer_key(&key, value),
        };
        Ok(())
    }

    fn visit_i8<E: serde::de::Error>(self, value: i8) -> Result<Self::Value, E> {
        self.visit_i32(value as i32)
    }

    fn visit_bool<E: serde::de::Error>(self, value: bool) -> Result<Self::Value, E> {
        match self.container {
            DeserializationTarget::NewValue { mut slot } => slot.construct_bool(value),
            DeserializationTarget::List { list } => list.append_bool(value),
            DeserializationTarget::Dict { dict, key } => dict.set_bool_key(&key, value),
        };
        Ok(())
    }

    fn visit_i64<E: serde::de::Error>(self, value: i64) -> Result<Self::Value, E> {
        // Here we match the behavior of the Chromium C++ JSON parser,
        // which will attempt to create an integer base::Value but will
        // overflow into a double if needs be.
        // (See JSONParser::ConsumeNumber in json_parser.cc for equivalent,
        // and json_reader_unittest.cc LargerIntIsLossy for a related test).
        match i32::try_from(value) {
            Ok(value) => self.visit_i32(value),
            Err(_) => self.visit_f64(value as f64),
        }
    }

    fn visit_u64<E: serde::de::Error>(self, value: u64) -> Result<Self::Value, E> {
        // See visit_i64 comment.
        match i32::try_from(value) {
            Ok(value) => self.visit_i32(value),
            Err(_) => self.visit_f64(value as f64),
        }
    }

    fn visit_f64<E: serde::de::Error>(self, value: f64) -> Result<Self::Value, E> {
        match self.container {
            DeserializationTarget::NewValue { mut slot } => slot.construct_double(value),
            DeserializationTarget::List { list } => list.append_double(value),
            DeserializationTarget::Dict { dict, key } => dict.set_double_key(&key, value),
        };
        Ok(())
    }

    fn visit_str<E: serde::de::Error>(self, value: &str) -> Result<Self::Value, E> {
        match self.container {
            DeserializationTarget::NewValue { mut slot } => slot.construct_string(value),
            DeserializationTarget::List { list } => list.append_string(value),
            DeserializationTarget::Dict { dict, key } => dict.set_string_key(&key, value),
        };
        Ok(())
    }

    fn visit_borrowed_str<E: serde::de::Error>(self, value: &'de str) -> Result<Self::Value, E> {
        match self.container {
            DeserializationTarget::NewValue { mut slot } => slot.construct_string(value),
            DeserializationTarget::List { list } => list.append_string(value),
            DeserializationTarget::Dict { dict, key } => dict.set_string_key(&key, value),
        };
        Ok(())
    }

    fn visit_string<E: serde::de::Error>(self, value: String) -> Result<Self::Value, E> {
        self.visit_str(&value)
    }

    fn visit_none<E: serde::de::Error>(self) -> Result<Self::Value, E> {
        match self.container {
            DeserializationTarget::NewValue { mut slot } => slot.construct_none(),
            DeserializationTarget::List { list } => list.append_none(),
            DeserializationTarget::Dict { dict, key } => dict.set_none_key(&key),
        };
        Ok(())
    }

    fn visit_unit<E: serde::de::Error>(self) -> Result<Self::Value, E> {
        self.visit_none()
    }

    fn visit_map<M>(mut self, mut access: M) -> Result<Self::Value, M::Error>
    where
        M: MapAccess<'de>,
    {
        let mut value = match self.container {
            DeserializationTarget::NewValue { ref mut slot } => slot.construct_dict(),
            DeserializationTarget::List { list } => list.append_dict(),
            DeserializationTarget::Dict { dict, key } => dict.set_dict_key(&key),
        };
        // If it were exposed by values.h, we could use access.size_hint()
        // to give a clue to the C++ about the size of the desired map.
        while let Some(key) = access.next_key::<String>()? {
            access.next_value_seed(ValueVisitor {
                container: DeserializationTarget::Dict { dict: &mut value, key },
                recursion_depth_check: self.recursion_depth_check.recurse()?,
            })?;
        }
        Ok(())
    }

    fn visit_seq<S>(mut self, mut access: S) -> Result<Self::Value, S::Error>
    where
        S: SeqAccess<'de>,
    {
        let mut value = match self.container {
            DeserializationTarget::NewValue { ref mut slot } => slot.construct_list(),
            DeserializationTarget::List { list } => list.append_list(),
            DeserializationTarget::Dict { dict, key } => dict.set_list_key(&key),
        };
        if let Some(size_hint) = access.size_hint() {
            value.reserve_size(size_hint);
        }
        while let Some(_) = access.next_element_seed(ValueVisitor {
            container: DeserializationTarget::List { list: &mut value },
            recursion_depth_check: self.recursion_depth_check.recurse()?,
        })? {}
        Ok(())
    }
}

impl<'de, 'elem, 'container> DeserializeSeed<'de> for ValueVisitor<'elem, 'container> {
    // Because we are deserializing into a pre-existing object.
    type Value = ();

    fn deserialize<D>(self, deserializer: D) -> Result<Self::Value, D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(self)
    }
}
