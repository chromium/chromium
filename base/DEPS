include_rules = [
  "+third_party/ashmem",
  "+third_party/apple_apsl",
  "+third_party/boringssl/src/include",
  "+third_party/ced",
  "+third_party/libevent",
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

  # These are implicitly brought in from the root, and we don't want them.
  "-ipc",
  "-url",

  # ICU dependendencies must be separate from the rest of base.
  "-i18n",

  # //base/util can use //base but not vice versa.
  "-util",
]
