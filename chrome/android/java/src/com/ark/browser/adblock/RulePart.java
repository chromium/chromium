package com.ark.browser.adblock;

public class RulePart {

    private RuleNode.RuleType type;
    private String value;

    public RuleNode.RuleType getType() {
        return this.type;
    }

    public void setType(RuleNode.RuleType type) {
        this.type = type;
    }

    public String getValue() {
        return this.value;
    }

    public void setValue(String value) {
        this.value = value;
    }

    public RulePart(RuleNode.RuleType type, String value) {
        this.type = type;
        this.value = value;
    }

}
