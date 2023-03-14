# Dummy libgcc.a

This directory contains an empty `libgcc.a` file for use with the Rust toolchain
when targeting Android.

The Rust compiler unconditionally attempts to link libgcc when targeting
Android, so `-lgcc` appears in the linker command when producing a `.so` file
for Android. Rustc expects libgcc will provide unwinding support, however we
already use libunwind, which we explicitly link ourselves through `ldflags`, to
provide this. Our Android toolchain has no libgcc present, which means Rustc
driven linking targeting Android fails with a missing library that we don't
actually want to use.

This same issue occurs for other consumers of rustc, when building for targets
without a libgcc, and the solution is to [give rustc an empty `libgcc.a` file](
https://www.reddit.com/r/rust/comments/jst1kk/building_rust_without_linking_against_libgcc/).

Therefore this directory contains an empty `libgcc.a` file, and on Android we
include this directory in the linker paths so that the rustc-driven linking step
can succeed without needing a real libgcc.
