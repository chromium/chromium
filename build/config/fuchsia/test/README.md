## CMX Fragments

This directory contains the cmx fragments that are required for running
Fuchsia tests hermetically. Tests start from `minimum_capabilities.test-cmx`
and add additional capabilities as necessary by providing the
`additional_manifest_fragments` argument. Some fragments are explained in detail
below:

### General Purpose Fragments

#### font_capabilities.test-cmx
For tests that test fonts by providing `fuchsia.fonts.Provider`.

#### jit_capabilities.test-cmx
Required by tests that execute JavaScript. Should only be required in a small
number of tests.

#### minimum_capabilites.test-cmx
Capabilities required by anything that uses `//base/test`, used as the base
fragment for all test suites.

#### read_debug_data.test-cmx
Required by tests that need access to its debug directory. Should only be
required in a small number of tests.

#### test_logger_capabilities.test-cmx
For tests that test logging functionality by providing `fuchsia.logger.Log`.

### WebEngine Fragments
The following fragments are specific to WebEngine functionality as documented
documentation at
https://fuchsia.dev/reference/fidl/fuchsia.web#CreateContextParams and
https://fuchsia.dev/reference/fidl/fuchsia.web#ContextFeatureFlags.
Any test-specific exceptions are documented for each file.

#### audio_capabilities.test-cmx
Corresponds to the `AUDIO` flag. Required for enabling audio input and output.

#### network_capabilities.test-cmx
Corresponds to the `NETWORK` flag. Required for enabling network access. Note
that access to the root SSL certificates is not needed if ContextProvider is
used to launch the `Context`. The `fuchsia.device.NameProvider` dependency comes
from fdio.

#### present_view_capabilities.test-cmx
Services that are needed to render web content in a Scenic view and present it.
Most services are required per the FIDL documentation.
`fuchsia.ui.policy.Presenter` is additionally required by tests that create
views.

#### vulkan_capabilities.test-cmx
Corresponds to the `VULKAN` flag. Required for enabling GPU-accelerated
rendering of the web content.

#### web_engine_required_capabilities.test-cmx
Contains services that need to be present when creating a
`fuchsia.web.Context`. Note that the `fuchsia.scheduler.ProfileProvider` service
is only used in tests that encounter memory pressure code.

#### web_instance_host_capabilities.test-cmx
Contains services that need to be present to use `WebInstanceHost`.