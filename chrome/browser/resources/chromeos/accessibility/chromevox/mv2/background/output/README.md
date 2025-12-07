# Output in ChromeVox

ChromeVox has a few different ways of producing output. Sometimes, speech
is provided directly to `Output.withString()` -- after internationalizing
via `Msgs.getMsg()`. This at first seems like the right approach for most
situations, but the `Output` class has developed a sophisticated method
of automatically determining the output strings, based on the change in the
`CursorRange` that ChromeVox is focused on and the type of event that's
occurring (which could be navigation, or any
`chrome.automation.EventType`).

## Using `Output` to Produce Speech

During navigation, output is generated from the current and previous ranges
without needing to be explicitly specified. It uses announcement patterns
that are specified in the file `output_rules.js` in a special markup language.
It also navigates the ancestors of both ranges to make appropriate announcements
for any nodes that have been entered or exited, again using rules from
`output_rules.js`.

When an `AutomationEvent` is received, output is produced by passing the event
and the current range. These announcements are also specified in
`output_rules.js`.