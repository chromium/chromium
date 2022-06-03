# Rust toolchain

Chrome currently uses an experimental Rust toolchain built by the Android
team, which supports only Linux and Android builds.

To build Rust code on other platforms for development/experimentation, add the
following to your `gn args`:

```
use_unverified_rust_toolchain=true
rust_bin_dir="<path-to>/.cargo/bin"
```

## Using VSCode, rust-analyzer etc.

Any IDE which supports rust-analyser should be able to ingest metadata from gn
about the structure of our Rust project. Do this:

* `gn gen out/Debug/ --export-rust-project`
* `ln -s out/Debug/rust-project.json rust-project.json`, i.e. symlink the
  `rust-project.json` file to the root of the Chromium src directory.
