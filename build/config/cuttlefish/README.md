# Cuttlefish GN Build Flags
Note: GN Flags are Build time flags
You can get a comprehensive list of all arguments supported by gn by running the
command gn args --list out/some-directory (the directory passed to gn args is
required as gn args will invokes gn gen to generate the build.ninja files).
## is_cuttlefish (BUILDFLAG(PLATFORM_CUTTLEFISH))
Flag for building chromium for Cuttlefish devices.

