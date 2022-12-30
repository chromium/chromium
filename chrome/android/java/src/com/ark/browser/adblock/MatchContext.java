package com.ark.browser.adblock;

public class MatchContext {

    private int counter;
    private long deadline;
    private long duration;
    private int freq;
    private boolean genericBock;
    private int isDomainRule;
    private RuleNode location;

    public int getCounter() {
        return this.counter;
    }

    public void setCounter(int counter) {
        this.counter = counter;
    }

    public int getFreq() {
        return this.freq;
    }

    public void setFreq(int freq) {
        this.freq = freq;
    }

    public long getDuration() {
        return this.duration;
    }

    public void setDuration(long duration) {
        this.duration = duration;
    }

    public long getDeadline() {
        return this.deadline;
    }

    public void setDeadline(long deadline) {
        this.deadline = deadline;
    }

    public RuleNode getLocation() {
        return this.location;
    }

    public void setLocation(RuleNode location) {
        this.location = location;
    }

    public boolean isGenericBock() {
        return this.genericBock;
    }

    public void setGenericBock(boolean genericBock) {
        this.genericBock = genericBock;
    }

    public int getIsDomainRule() {
        return this.isDomainRule;
    }

    public void setIsDomainRule(int isDomainRule) {
        this.isDomainRule = isDomainRule;
    }

    public boolean canContinue(RuleNode n) {
        if (this.freq <= 0) {
            return true;
        }
        this.counter++;
        if (this.counter < this.freq) {
            return true;
        }
        boolean stop;
        this.counter = 0;
        long now = System.currentTimeMillis();
        if (now >= this.deadline) {
            stop = true;
        } else {
            stop = false;
        }
        if (stop) {
            this.location = n;
            this.duration += now - this.deadline;
        }
        if (stop) {
            return false;
        }
        return true;
    }


}
