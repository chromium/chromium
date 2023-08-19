Ash
---
Ash is the "Aura Shell", the window manager and system UI for Chrome OS.
Ash uses the views UI toolkit (e.g. views::View, views::Widget, etc.) backed
by the aura native widget and layer implementations.

Dependencies
------------
Ash sits below chrome in the dependency graph (i.e. it cannot depend on code
in //chrome). For historical reasons, ash has multiple dependency levels:

*   General //ash code is on top (//ash/system, //ash/wm, etc.)
*   //ash/components sit below //ash, see [README](/ash/components/README.md)
*   //ash/constants sit near the bottom of the dependency graph, see
    [README](/ash/constants/README.md)

Access to Ash internals is controlled by DEPS files. Unless explicitly allowed
by DEPS, code outside Ash should depend on the interfaces in //ash/public. Check
with [OWNERS](/ash/OWNERS) if you have questions.

Tests
-----
Tests should be added to the ash_unittests target.

Tests can bring up most of the ash UI and simulate a login session by deriving
from AshTestBase. This is often needed to test code that depends on ash::Shell
and the controllers it owns.

Test support code (TestFooDelegate, FooControllerTestApi, etc.) lives in the
same directory as the class under test (e.g. //ash/foo rather than //ash/test).
Test code uses namespace ash; there is no special "test" namespace.

Prefs
-----
Ash supports both per-user prefs and device-wide prefs. These are called
"profile prefs" and "local state" to match the naming conventions in chrome. Ash
also supports "signin screen" prefs, bound to a special profile that allows
users to toggle features like spoken feedback at the login screen.

Pref names are in //ash/public/cpp so that code in chrome can also use the
names. Prefs are registered in the classes that use them because those classes
have the best knowledge of default values.

Historical notes
----------------
Ash shipped on Windows for a couple years to support Windows 8 Metro mode.
Windows support was removed in 2016.

The mash (some times called mus-ash or mustash) project was an effort to move
ash into its own process and the browser in its own process. Communication
between the two processes was done over mojo. Windowing was done using the
window-service (some times called mus), which ran with Ash. Many of the mojo
interfaces have been converted to pure virtual interfaces, with the
implementation in ash. The mash project was stopped around 4/2019.

"Shelf party" was an Easter-egg feature that made shelf icons fly around the
screen. It did not launch and [was removed](http://b/284500985) in June 2023.
