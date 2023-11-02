Desktop New Tab Page Bug Triage Process
=======================================

last update: 04-19-2022

The current triage process owner is `mahmadi@`.

Instructions for Chrome Desktop NTP engineers participating in bug triage rotation.

### Goals and Expectations:

Every week, one engineer on the Desktop NTP team is responsible for triaging the
newly reported **Unconfirmed**, **Untriaged**, and **Available** bugs filed
against Desktop NTP. Every newly reported bug, should be preferably triaged
within one week. This includes understanding the issue, recreating the issue if
necessary, marking it as a duplicate if applicable, revising the status, setting
priority, assigning the appropriate labels and component (incl. handing off to
another component), and preferably assigning to an owner, if possible.

Some bugs may already be **Assigned** to an owner but miss the appropriate
labels. Those bugs are still considered untriaged and the responsible engineer
should ensure the bug owner applies the appropriate labels, component, and
priority.

If a bug is deemed to have a high priority or is highly visible or disruptive to
the user, the responsible engineer should inform the team (incl. the PM) so it
can be prioritized accordingly.

In addition to the newly reported bugs, the responsible engineer should also
attempt to triage at least two bugs from [go/ntp-triage](http://goto/ntp-triage)
and [go/ntp-triage-os](http://goto/ntp-triage) in that week. As of today these
two lists have less than 100 bugs combined. If two of these bugs are triaged
every week, there will be no untriaged NTP bugs by the end of the year!

### Process:

#### There are two ways to stay on top of the newly reported bugs:

1. Set up an email alert for the following queries in
[Monorail](https://bugs.chromium.org/p/chromium/issues/list). Or,
   1. component:UI>Browser>NewTabPage OS=Windows,Mac,Linux,Chrome
   2. component:UI>Browser>NewTabPage -has:OS (bugs that miss the OS label)
2. Actively poll untriaged bugs at [go/ntp-triage](http://goto/ntp-triage) and
[go/ntp-triage-os](http://goto/ntp-triage).

#### For every bug being triaged:

* Add either `ntp-backlog` (should be looked into) OR `ntp-icebox` (can be
ignored for now) label.
* If ntp-icebox set Pri to 3
* If ntp-backlog set Pri to:
  * 0 (needs to be fixed immediately)
  * 1 (needs to be fixed by certain, usually next, milestone)
  * 2 (nice to have for certain milestone, or affects unlaunched feature)
* Add an appropriate `ntp-epic-*` label (see
[go/ntp-epics](http://goto/ntp-epics) for currently used ones)
* Make sure the correct OS labels are set.
