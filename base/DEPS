include_rules = [
  "+third_party/ashmem",
  "+third_party/apple_apsl",
  "+third_party/boringssl/src/include",
  "+third_party/ced",
  "+third_party/libunwindstack/src/libunwindstack/include",
  "+third_party/lss",
  "+third_party/modp_b64",
  "+third_party/perfetto/include",
  "+third_party/perfetto/protos/perfetto",
  "+third_party/tcmalloc",
  "+third_party/test_fonts",

  # These are implicitly brought in from the root, and we don't want them.
  "-ipc",
  "-url",

  # ICU dependendencies must be separate from the rest of base.
  "-i18n",

  # //base/util can use //base but not vice versa.
  "-util",
]
