This directory contains code for handling accelerators in Ash. The following
documents the flow of accelerators.

1. wm::AcceleratorFilter() sees events first as it's a pre-target handler on
Shell.
2. wm::AcceleratorFilter calls to PreTargetAcceleratorHandler.
3. PreTargetAcceleratorHandler handles accelerators that need to be handled
early on, such as system keys. This does not include accelerators such as
control-n (for new window).
4. If focus is on a Widget, then views handles the accelerator.
5. Views does normally processing first (meaning sends to the focused view). If
the focused view doesn't handle the event, then Views sends to the
FocusManager.
6. FocusManager::OnKeyEvent() calls
PostTargerAcceleratorHandler::ProcessAccelerator().
7. PostTargerAcceleratorHandler::ProcessAccelerator() calls to
Ash's AcceleratorController.

Steps 1-3 give Ash the opportunity to have accelerators before the target
(these are often referred to as pre-target accelerators). Step 4-5
allows the target to handle the accelerator. Step 6-7 allow for
post-target accelerators (accelerators that only occur if the target does not
handle the accelerator).

Steps 4-7 differ if focus is on a remote window (a window created at the
request of a client connecting by way of the WindowService). If focus is on
a remote window, then step 4-7 are replaced with:

1. WindowService waits for response from remote client.
2. If remote client does not handle the event, WindowService calls
WindowServiceDelegate::OnUnhandledKeyEvent().
3. Ash's WindowServiceDelegateImpl::OnUnhandledKeyEvent() calls to
AcceleratorController::Process(), which handles the post-target processing
phase.
