# Chrome on Android Feature Targets

This is the top-level directory for various feature targets for chrome on
android. Each subdirectory should be one self-contained feature including all
the source files, resource files, string translations that are part of that
feature. See the directory structure for
[keyboard_accessory](keyboard_accessory) as an example. Some of these features
are dynamic feature modules, and others are plain features that are in the base
chrome module.

There are some useful GN templates in this top-level directory. For example:
[android_library_factory_tmpl.gni](android_library_factory_tmpl.gni) contains
a template to make generating empty build-time factories easier. This allows
`chrome_java` to not depend on the internal implementation of a feature but
rather call these generated factories. The specifics are documented in the GN
template file.
