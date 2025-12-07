// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.location_rewriter;

import org.objectweb.asm.commons.AdviceAdapter;

import java.util.Objects;

/** A method visitable by an ASM method visitor. */
public final class VisitableMethod {
    public final int opcode;
    public final String owner;
    public final String name;
    public final String descriptor;
    public final boolean isInterface;

    public VisitableMethod(
            int opcode, String owner, String name, String descriptor, boolean isInterface) {
        this.opcode = opcode;
        this.owner = owner;
        this.name = name;
        this.descriptor = descriptor;
        this.isInterface = isInterface;
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof VisitableMethod that
                && this.opcode == that.opcode
                && this.owner.equals(that.owner)
                && this.name.equals(that.name)
                && this.descriptor.equals(that.descriptor)
                && this.isInterface == that.isInterface;
    }

    @Override
    public int hashCode() {
        return Objects.hash(opcode, owner, name, descriptor, isInterface);
    }

    public void visit(AdviceAdapter visitor) {
        visitor.visitMethodInsn(opcode, owner, name, descriptor, isInterface);
    }
}
