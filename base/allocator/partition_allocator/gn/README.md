# PartitionAlloc standalone GN config

This directory contains a GN configuration to build partition_alloc as a
standalone library.

This is not an official product that is supported by the Chromium project. There
are no guarantees that this will work in the future, or that it will work in
all configurations. There are no commit queue or trybots using it.

This is useful for verifying that partition_alloc can be built as a library, and
discover the formal dependencies that partition_alloc has on the rest of the
Chromium project. This is not intended to be used in production code, and is not

This is also provided as a convenience for chromium developers working on
partition_alloc who want to iterate on partition_alloc without having to build
the entire Chromium project.

/!\ This is under construction. /!\

## Building

As of today, partition_alloc still depends on chromium's
`//build/build_config.h`, so a symbolic link is needed to make it work.
```sh
ln -s /path/to/chromium/src/build/ src/build
```

Then you can build partition_alloc as a standalone library by running:
```sh
gn gen out/Default
autoninja -C out/Default
```

## Supported configurations:

### Platforms
- Linux

### Toolchains
- Clang
