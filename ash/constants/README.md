# ash/constants

This directory contains constants used by Chrome OS. It is a very low-level
component and should only depend on //base. It should not contain any
logic, except for simple helper functions like IsFooEnabled(). For example,
while it can contain pref names, it should not do pref registration, because
pref registration requires a dependency on //components.

It lives in //ash because these constants are used by Chrome OS system UI as
well as the current/legacy built-in chrome browser. This is "ash-chrome" in the
terminology of the [Lacros project](/docs/lacros.md).

Code in this directory used to live in //chromeos/constants. That directory is
being re-purposed for constants shared between the lacros-chrome browser and
the ash-chrome system UI binary. Both those binaries run on Chrome OS.
