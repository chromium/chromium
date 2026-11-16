# Protos
`Resources_pb2.py` and `Configuration_pb2.py` are generated from
`Resources.proto` and `Configuration.proto` from the Android repo. They are
found in the `frameworks/base/tools/aapt2/` directory. To regenerate these if
there are changes, run this command from the root of an Android checkout:

   protoc --python_out=some_dir frameworks/base/tools/aapt2/Resources.proto \
      frameworks/base/tools/aapt2/Configuration.proto

Then copy the resulting \*pb2.py files from some_dir here. To make sure
Resources_pb2.py is able to import Configuration_pb2.py, replace the
"from frameworks.base.tools.aapt2" portion of the import statement with
"from ." so it will instead be imported from the current directory.

`deps_pb2.py` is generated from Bazel's `src/main/protobuf/deps.proto` (used by
Turbine's `--output_deps` `.jdeps` output):

   //third_party/android_build_tools/protoc/cipd/protoc \
      --python_out=build/android/gyp/proto deps.proto
