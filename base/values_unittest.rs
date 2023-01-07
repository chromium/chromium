use base::{NewValueSlotForTesting, ValueSlotRef};
use rust_gtest_interop::prelude::*;

#[gtest(RustValuesTest, AllocDealloc)]
fn test_alloc_dealloc() {
    NewValueSlotForTesting();
}

#[gtest(RustValuesTest, StartsNone)]
fn test_starts_none() {
    let mut v = NewValueSlotForTesting();
    let v = ValueSlotRef::from(v.pin_mut());
    expect_eq!(format!("{:?}", v), "(empty)");
}

#[gtest(RustValuesTest, SetDict)]
fn test_set_dict() {
    let mut v = NewValueSlotForTesting();
    let mut v = ValueSlotRef::from(&mut v);
    let mut d = v.construct_dict();
    d.set_string_key("fish", "skate");
    d.set_none_key("antlers");
    d.set_bool_key("has_lungs", false);
    d.set_integer_key("fins", 2);
    d.set_double_key("bouyancy", 1.0);
    let mut nested_list = d.set_list_key("scales");
    nested_list.append_string("sea major");
    let mut nested_dict = d.set_dict_key("taxonomy");
    nested_dict.set_string_key("kingdom", "animalia");
    nested_dict.set_string_key("phylum", "chordata");
    // TODO(crbug.com/1282310): Use indoc to make this neater.
    expect_eq!(
        format!("{:?}", v),
        "{\n   \
            \"antlers\": null,\n   \
            \"bouyancy\": 1.0,\n   \
            \"fins\": 2,\n   \
            \"fish\": \"skate\",\n   \
            \"has_lungs\": false,\n   \
            \"scales\": [ \"sea major\" ],\n   \
            \"taxonomy\": {\n      \
                \"kingdom\": \"animalia\",\n      \
                \"phylum\": \"chordata\"\n   \
            }\n\
        }\n"
    );
}

#[gtest(RustValuesTest, SetList)]
fn test_set_list() {
    let mut v = NewValueSlotForTesting();
    let mut v = ValueSlotRef::from(&mut v);
    let mut l = v.construct_list();
    l.reserve_size(5);
    l.append_bool(false);
    l.append_none();
    l.append_double(2.0);
    l.append_integer(4);
    let mut nested_list = l.append_list();
    nested_list.append_none();
    let mut nested_dict = l.append_dict();
    nested_dict.set_string_key("a", "b");
    l.append_string("hello");
    expect_eq!(
        format!("{:?}", v),
        "[ false, null, 2.0, 4, [ null ], {\n   \
            \"a\": \"b\"\n\
        }, \"hello\" ]\n"
    );
}

fn expect_simple_value_matches<F>(f: F, expected: &str)
where
    F: FnOnce(&mut ValueSlotRef),
{
    let mut v = NewValueSlotForTesting();
    let mut v = ValueSlotRef::from(&mut v);
    f(&mut v);
    expect_eq!(format!("{:?}", v).trim_end(), expected);
}

#[gtest(RustValuesTest, SetSimpleOptionalValues)]
fn test_set_simple_optional_values() {
    expect_simple_value_matches(|v| v.construct_none(), "null");
    expect_simple_value_matches(|v| v.construct_bool(true), "true");
    expect_simple_value_matches(|v| v.construct_integer(3), "3");
    expect_simple_value_matches(|v| v.construct_double(3.1), "3.1");
    expect_simple_value_matches(|v| v.construct_string("a"), "\"a\"");
}

#[gtest(RustValuesTest, ReuseSlot)]
fn test_reuse_slot() {
    let mut v = NewValueSlotForTesting();
    let mut v = ValueSlotRef::from(&mut v);
    v.construct_none();
    let mut d = v.construct_dict();
    d.set_integer_key("a", 3);
    v.construct_integer(7);
    expect_eq!(format!("{:?}", v).trim_end(), "7");
}
