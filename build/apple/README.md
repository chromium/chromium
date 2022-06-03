# About

`//build/apple` contains:
  * GN templates and configurations shared by Apple platforms
  * Python build scripts shared by Apple platforms

This directory should only contain templates, configurations and scripts
that are used exclusively on Apple platforms (currently iOS and macOS).
They must also be independent of the specific platform.

If a template, configuration or script is limited to only iOS or macOS,
then they should instead be located in `//build/ios` or `//build/mac`.
