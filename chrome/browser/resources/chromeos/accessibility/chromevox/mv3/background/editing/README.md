# Text Editing in ChromeVox

Text edit handling begins in `DesktopAutomationHandler`. When a focus event
or load event is fired, the method `createTextEditHandlerIfNeeded_` determines
if the current range is within an editable text context. If so, it creates a
`TextEditHandler`, which in turn creates an `AutomationEditableText` (or
`AutomationRichEditableText`, if it's a richly editable text field). The
`TextEditHandler` receives `EditableChanged` events from `DesktopAutomationNode`
and forwards the relevant information (e.g. `AutomationIntent`s) to the
`AutomationEditableText`.

`AutomationEditableText` does the bulk of the logic, tracking the cursor and
selection within the editable as well as performing actions on the text.