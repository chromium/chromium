---
name: general/declare-installer-dependencies-when-unbundling-linux-tools
rubric: correctness
severity: should_fix
---

**Rule.** When unbundling third-party utilities or tools from Linux installer
packages, verify all companion installer packages (such as Chrome Remote Desktop
/ remoting) that invoke the tools at runtime. Update their package manifests
(such as Debian control files or RPM additional_deps) to declare explicit
dependencies on the system packages providing those utilities.

**Why.** When bundled tools (such as xdg-settings or desktop integration
utilities) are removed from main installer staging directories, companion
packages that execute those tools directly fail at runtime unless their package
manifests explicitly declare system dependencies on the packages providing them.

## Examples

Bad:

```cpp
# remoting/host/installer/linux/debian/control
Package: chrome-remote-desktop
Depends: adduser, libsystemd0, psmisc, python3, xvfb, ${shlibs:Depends}, ${misc:Depends}
```

Good:

```cpp
# remoting/host/installer/linux/debian/control
Package: chrome-remote-desktop
Depends: adduser, libsystemd0, psmisc, python3, xdg-utils (>= 1.0.2), xvfb, ${shlibs:Depends}, ${misc:Depends}
```
