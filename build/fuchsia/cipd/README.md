# CIPD recipes

The `//build/fuchsia/cipd` target generates a number of YAML files that are used to
produce archives that are uploaded to CIPD. The generated YAML files are stored
in the output directory under the path `gen/build/fuchsia/cipd/`.

## Example usage

The most recent package can be discovered by searching for the "canary" ref:

`$ cipd describe chromium/fuchsia/$PACKAGE_NAME-$TARGET_ARCH -version canary`
