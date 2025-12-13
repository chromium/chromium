# Backend config for Siso

This directory contains backend specific config for
[siso](https://chromium.googlesource.com/build/+/refs/heads/main/siso/)
build tool.

User needs to add `backend.star` that provides `backend` module
for `platform_properties` dict.  The dict provides platform type
as key (e.g. "default", "large"), and RBE properties (e.g.
"container-image", "OSFamily" etc).
Copy `template.star` to `backend.star` and edit property values.
