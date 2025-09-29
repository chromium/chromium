include_rules = [
  "+aidl/org/chromium/base",
  "+third_party/apple_apsl",
  "+third_party/boringssl/src/include",
  "+third_party/ced",
  "+third_party/fuzztest",
  # We are moving the old jni_generator to jni_zero, some references will remain
  # in //base.
  "+third_party/jni_zero",
  "+third_party/libunwindstack/src/libunwindstack/include",
  "+third_party/lss",
  "+third_party/modp_b64",
  "+third_party/perfetto/include",
  "+third_party/perfetto/protos/perfetto",
  # Conversions between base and Rust types (e.g. base::span <-> rust::Slice)
  # require the cxx.h header from cxx. This is only used if Rust is enabled
  # in the gn build; see //base/BUILD.gn's conditional dependency on
  # //build/rust:cxx_cppdeps.
  "+third_party/rust/cxx",
  "+third_party/test_fonts",
  # JSON Deserialization.
  "+third_party/rust/serde_json_lenient/v0_2/wrapper",
  "+third_party/zlib",

  # These are implicitly brought in from the root, and we don't want them.
  "-ipc",
  "-url",

  # ICU dependendencies must be separate from the rest of base.
  "-i18n",

  # //base/util can use //base but not vice versa.
  "-util",
]

specific_include_rules = {
  # To evaluate the performance effects of using absl's flat_hash_map.
  "supports_user_data\.cc": [
    "+third_party/abseil-cpp/absl/container/flat_hash_map.h",
  ]
}
