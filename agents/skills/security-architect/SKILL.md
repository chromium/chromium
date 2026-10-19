---
name: security-architect
description: |-
  Skill for assessing security architecture and suggesting improvements.
  - This skill is only useful for C++ code.
---

This skill is all about producing a security architecture audit report, with
suggestions for fixes, improvements, and hardenings. Bear in mind that this is
an *architecture* audit report, not a code audit report, so higher-level is
better, but we do need to suggest changes that would be feasible.

# Writing A Good Report

Lay out *at most five* improvements you can see to make to the security of the
target area. Interface and implementation improvements are both good. For each
improvement, give:

- A sketch of how to implement it
- Your assessment of how much impact it would have (if any) on clients of any
  interfaces you're changing
- Any risks or downsides you see to doing it

Don't make any actual code changes - you're just writing a report.

# Interface guidance

- Many security problems are caused by lifetime misunderstandings at interfaces
  so interfaces should prioritize clean transfer of ownership and simple
  ownership models.
- Interfaces should use spans in preference to (pointer, length) pairs.
- Mojo interfaces cross privilege boundaries, so you should be especially
  skeptical of any Mojo interface that gives a less-privileged end control of
  the lifetime of an object at the more-privileged end.
- Privilege gradients go: the browser process is most privileged, then the net
  and GPU processes, then the other utility processes, then the renderer.

# Implementation guidance

- Try hard to obey the Rule of Two: untrusted data may not be handled in unsafe
  ways in the browser process. In priority order:
  1. Avoid handling it in the browser process at all.
  2. Rewrite the handling to be safe (by doing it in Rust).
  3. Do the handling in a sandboxed helper process that the browser IPCs to.
